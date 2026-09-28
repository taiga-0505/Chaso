#include "CaptureMode.h"

#include "Common/EngineConfig.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include "../Game/Framework/GameSession.h"
#include "../Game/Scene/Scene.h"
#include "Common/Log/Log.h"
#include "Common/Water/WaterSurface.h"
#include "DebugBridge.h"
#include "Dx12/Dx12Core.h"
#include "ECS/CameraComponent.h"
#include "ECS/Entity.h"
#include "ECS/LightComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/RigidbodyComponent.h"
#include "ECS/ScriptableEntity.h"
#include "ECS/TransformComponent.h"
#include "ECS/WaterComponent.h"
#include "Input/Input.h"
#include "Render/RenderCommon.h"
#include "Render/RenderContext.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

// 撮影モードに入っているか / シーンを再生状態にしてほしいか。
// この2つだけは Release ビルドでも参照するのでガードの外に置く。
bool g_active = false;
bool g_wantsPlaying = false;
/// @brief マウスがゲーム画面の上にあるか（いまは字幕が無いので常に true）
bool g_gameHovered = true;

#if RC_ENABLE_IMGUI

float g_stepTime = 0.0f; ///< そのステップに入ってからの経過秒
float g_clock = 0.0f;    ///< 撮影モードに入ってからの累積秒（キャッシュの期限に使う）

// ===========================================================================
// ステップの定義
// ---------------------------------------------------------------------------
// 撮影順に並べる。週ごとにまとめてあるので、
// 「今週ぶんだけ撮る」ときは該当の週から始めればよい。
// ===========================================================================

/// @brief そのステップで何を出すか
enum class Kind {
  Info,        ///< 説明と汎用の実測値だけ
  Week1Scene,  ///< 第1週の題材（別シーン）へ飛ばす
  Pcf,         ///< C-04: PCF を自動で ON/OFF して見比べる
  Reflect,     ///< C-03: 波の反射を自動で ON/OFF して見比べる
  SceneFlow,   ///< A-02: Title → Game → Result → Title を順に飛ばす
  SpriteDigits,///< D-05: スコアの桁が別々に出ることを見せる
  Resources,   ///< D-04: ハンドルが回収されることを数で見せる
  Rail,        ///< T-20: ルート分岐
  Lights,      ///< A-05: レベル由来のライトとカメラ
  Rotation,    ///< 回転単位（度数法／ラジアン）
  Entities,    ///< D-07〜D-10: 保存で複製が増えないこと
  Buoyancy,    ///< C-01/C-02: 落として実測する
  Wave,        ///< A-03: ウェーブ戦闘
  Ship,        ///< T-15: 船の AI と浮力の流用
  Obstacles,   ///< D-01: 障害物のデータ化
};

/// @brief 撮影ステップ1件
struct Step {
  const char *week;   ///< いつの実装か
  const char *id;     ///< タスクID
  const char *title;  ///< 何を証明するのか
  const char *watch;  ///< 何を見れば分かるのか（撮影時のメモ。画面には出さない）
  const char *action; ///< F7 で何が起きるか。nullptr なら F7 は無効
  Kind kind;
};

const Step kSteps[] = {
    // ---------------- 第1週 ----------------
    {"第1週", "PostFX",
     "ポストエフェクト基盤と自作4種",
     "プレイヤーの Y が 0 を跨いだ瞬間に、適用スタックが "
     "LightShaft → Caustics → Underwater → Vignette → RadialBlur → ScreenDroplets "
     "へ 0.5 秒で組み替わる。水上へ戻すと該当パスが外れて描画負荷も消える。"
     "潜る・上がるを2往復して見せること。",
     nullptr, Kind::Info},
    {"第1週", "Anim",
     "スケルタルアニメ改良とボーンソケット・炎パーティクル",
     "CG4 シーンへ移動する。農夫モデルに Idle1 / Idle2 / Walk / Run / Attack の "
     "5状態を組んであり、放置5秒で待機モーションが切り替わる。"
     "0.2秒のクロスフェードで繋がること、右手の炎エミッタがボーンに追従することを見せる。",
     "CG4 シーンへ移動する", Kind::Week1Scene},

    // ---------------- 第2週 ----------------
    {"第2週", "C-04",
     "シャドウマップの PCF 対応",
     "2.5秒ごとに PCF を自動で切り替える。OFF のとき影の輪郭が階段状にギザつき、"
     "ON にすると滑らかになる。影の境界がはっきり出ている物（岩や船）を"
     "画面に大きく入れてから撮ること。",
     "自動切り替えを止める / 再開する", Kind::Pcf},
    {"第2週", "C-03",
     "障害物への波の反射（鏡像法）",
     "3秒ごとに反射を自動で切り替える。ON のとき岩の風上側にだけ波が立ち、"
     "その波が岩から離れる向きへ伝わっていく。OFF では岩の周りが平らなまま。"
     "岩を画面の中央に入れ、風上側が見える角度から撮ること。",
     "自動切り替えを止める / 再開する", Kind::Reflect},
    {"第2週", "A-02",
     "コアゲームループの結合",
     "Title → Game → Result → Title を一周させる。"
     "シーンをまたいでスコアと HP が引き継がれ、Result のランクバーが"
     "下の値と一致することを見せる。ここが今までの動画に写っていなかった。",
     "次のシーンへ進む", Kind::SceneFlow},
    {"第2週", "D-05",
     "スプライトの切り出し矩形が効かない不具合",
     "修正当時は Result のスコア桁（number.png の切り出し）で確認していたが、"
     "いまの Result はフォント描画（DrawString）なので桁スプライトは出ない。"
     "Result へ飛んで達成率・撃破数・被ダメージが GameSession の値どおりかを見る。",
     "値を入れて Result へ飛ぶ", Kind::SpriteDigits},
    {"第2週", "D-04",
     "死亡エンティティのハンドル解放漏れ",
     "基準を記録してから敵を倒す、またはシーンを一周する。"
     "使用中スプライトとライトの数が基準へ戻れば回収できている。"
     "増えたまま戻らないなら解放漏れが残っている。",
     "いまの数を基準として記録する", Kind::Resources},

    // ---------------- 第3週 ----------------
    {"第3週", "T-20",
     "ステージのルート分岐",
     "右キーを押しながら分岐点を通過すると水上ルートへ逸れ、共通のゴールで合流する。"
     "下のウェイポイント番号が飛ぶ瞬間と、直近に成立した分岐条件が証拠になる。"
     "終点では rail_finished が立ち、周回に入らないこと。",
     nullptr, Kind::Rail},
    {"第3週", "A-05",
     "レベルローダーの LIGHT / CAMERA 対応",
     "レベル JSON から読み込んだライトとカメラの内訳を出す。"
     "平行光源・点光源・スポット・エリアの4種と、カメラの画角・クリップ距離が"
     "データ側の値で入っていることが確認できる。",
     nullptr, Kind::Lights},
    {"第3週", "回転単位",
     "回転の単位の食い違いを解消",
     "エンジン内部はラジアン、Blender の出力もラジアン。"
     "度数法で書かれたデータはファイル単位のフラグで吸収する。"
     "下の一覧でラジアン値と度数換算が対応していれば、取り違えは起きていない。",
     nullptr, Kind::Rotation},
    {"第3週", "D-07〜10",
     "既存不具合の修正4件",
     "シーンを保存して開き直してもエンティティ数が増えないこと（保存のたびに"
     "レベル由来の複製が増える問題）。スクリプトを外した物体が"
     "無重力のまま残らないこと。下の数が一定なら直っている。",
     nullptr, Kind::Entities},
    {"第3週", "C-01/02",
     "浮力シミュレーションのパラメータ検証",
     "浮遊物を持ち上げて落とし、着水からの挙動をその場で実測する。"
     "2乗抵抗を入れる前は 12m 落下で余分に 3.94m 沈み、復帰に 6.22 秒かかっていた。"
     "いまは余分な沈み込みがほぼ 0、復帰は 1.4 秒前後で、落下高さを変えても変わらない。",
     "持ち上げて落とす（計測開始）", Kind::Buoyancy},

    // ---------------- 第4週（今週） ----------------
    {"第4週", "A-03",
     "ウェーブ戦闘の構築",
     "ウェーブを開始すると敵が湧き、レールが停止する。"
     "残敵数が減っていき、全滅した瞬間に累計クリア数が増えてレールが再進行する。"
     "この一連が今週いちばん見せたいところ。",
     "ウェーブを開始する", Kind::Wave},
    {"第4週", "T-15",
     "新規敵タイプ「船」の AI と挙動",
     "船は Y と傾きを自分で動かさず浮力に任せているので、波に乗って上下し傾く。"
     "撃沈しても即消滅せず、浸水（flooding）で密度比が上がって沈んでいく。"
     "沈みきるまでの数秒が写るように撮ること。",
     "いちばん近い船を撃沈させる", Kind::Ship},
    {"第4週", "D-01",
     "水面シェーダの障害物座標のデータ化",
     "以前はレンダラ内に座標がハードコードされていた。"
     "いまは岩の Transform から拾っているので、岩を動かすと下の座標も追従する。"
     "描画の反射と当たり判定の水面が同じリストを見ている。",
     nullptr, Kind::Obstacles},
};

constexpr int kStepCount = static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0]));

// ===========================================================================
// 状態
// ===========================================================================

int g_step = 0;
bool g_autoToggle = true; ///< PCF / 反射の自動切り替え
bool g_toggleOn = false;  ///< 自動切り替えの現在の側

/// @brief A-02 のシーン送りで次に飛ぶ先
int g_sceneFlowIndex = 0;
const char *const kSceneFlow[] = {"Title", "Game", "Result", "Title"};
constexpr int kSceneFlowCount =
    static_cast<int>(sizeof(kSceneFlow) / sizeof(kSceneFlow[0]));

/// @brief D-04 の基準値
struct Baseline {
  bool taken = false;
  size_t spriteInUse = 0;
  int pointLights = 0;
  int spotLights = 0;
  int areaLights = 0;
  size_t entities = 0;
};
Baseline g_baseline;

/// @brief 浮力の落下計測
/// @details 「12m 落下 → 余分な沈み込み 0.00m → 1.38 秒で復帰」という
///          報告書の数値を、実機でその場で出し直すための計測。
struct DropMeasure {
  enum class Phase { Idle, Falling, Submerged, Done };
  Phase phase = Phase::Idle;
  uint32_t targetId = 0;   ///< 計測対象のエンティティID
  float dropHeight = 12.0f;///< 静水面から何 m 上へ持ち上げるか
  float halfHeight = 1.0f; ///< 対象の半分の高さ（完全水没の判定に使う）
  float startY = 0.0f;
  float waterY = 0.0f;
  float deepest = 0.0f;    ///< 完全水没からさらに沈んだ最大量(m)
  float submergedTime = 0.0f;
  float recoverTime = 0.0f;///< 完全水没してから水面へ戻るまで(s)
  bool everFullySubmerged = false;
};
DropMeasure g_drop;

// ===========================================================================
// 小道具
// ===========================================================================

/// @brief 指定した型名のスクリプトを持つエンティティを集める
/// @details ゲームプレイのスクリプトは .cpp 内ローカル型でヘッダが無いため、
///          型では引けない。登録名（文字列）で絞り込む。
std::vector<Entity *> FindWithScript(Scene *scene, const char *typeName) {
  std::vector<Entity *> found;
  if (!scene || !typeName) return found;
  for (const auto &e : scene->GetEntities()) {
    if (!e) continue;
    auto *nsc = e->GetComponent<NativeScriptComponent>();
    if (!nsc) continue;
    for (const auto &entry : nsc->scripts) {
      if (entry.scriptTypeName.find(typeName) != std::string::npos) {
        found.push_back(e.get());
        break;
      }
    }
  }
  return found;
}

/// @brief エンティティから指定した型名のスクリプト実体を引く
ScriptableEntity *GetScript(Entity *e, const char *typeName) {
  if (!e || !typeName) return nullptr;
  auto *nsc = e->GetComponent<NativeScriptComponent>();
  if (!nsc) return nullptr;
  for (const auto &entry : nsc->scripts) {
    if (entry.scriptTypeName.find(typeName) != std::string::npos) {
      return entry.instance;
    }
  }
  return nullptr;
}

/// @brief スクリプトの設定値を JSON で取り出す（型が見えないための迂回）
nlohmann::json SafeSerialize(ScriptableEntity *s) {
  if (!s) return nlohmann::json::object();
  try {
    return s->Serialize();
  } catch (...) {
    return nlohmann::json::object();
  }
}

/// @brief SafeSerialize の結果を 0.25 秒だけ使い回す
/// @param e 対象のエンティティ
/// @param typeName スクリプトの登録名
/// @details 字幕は毎フレーム描くが、レールのウェイポイント一覧のように
///          そこそこ大きい JSON を 60fps で組み直すと、
///          録画にフレーム落ちとして写る可能性がある。
/// @note キーはエンティティIDと型名の両方で見る。IDだけにすると、
///       1つのエンティティに複数のスクリプトが載っている場合（船＝
///       ShipEnemyScript ＋ BuoyancyScript）に別のスクリプトの
///       JSON を返してしまう。
/// @warning typeName はポインタで比較している。呼び出しは文字列リテラルに
///          限ること。std::string::c_str() を渡すと、解放後に同じアドレスが
///          再利用されたときに別の型でキャッシュヒットする。
const nlohmann::json &CachedSerialize(Entity *e, const char *typeName) {
  static nlohmann::json cache = nlohmann::json::object();
  static uint32_t cachedId = 0;
  static const char *cachedType = nullptr;
  static float nextRefresh = -1.0f;

  const uint32_t id = e ? e->GetId() : 0u;
  if (cachedId != id || cachedType != typeName || g_clock >= nextRefresh) {
    cache = SafeSerialize(GetScript(e, typeName));
    cachedId = id;
    cachedType = typeName;
    nextRefresh = g_clock + 0.25f;
  }
  return cache;
}

/// @brief シーンから水面の波パラメータを組み立てる
/// @return 水面エンティティが見つかれば true
bool BuildWaterParams(Scene *scene, RC::WaterWaveParams &out) {
  if (!scene) return false;
  for (const auto &e : scene->GetEntities()) {
    if (!e) continue;
    auto *w = e->GetComponent<WaterComponent>();
    if (!w) continue;
    out.waveHeight = w->waveHeight;
    out.waveSpeed = w->waveSpeed;
    out.waveFreq = w->waveFreq;
    out.waveHeight2 = w->waveHeight2;
    out.waveSpeed2 = w->waveSpeed2;
    out.waveFreq2 = w->waveFreq2;
    out.waveSteepness = w->waveSteepness;
    if (auto *tr = e->GetComponent<TransformComponent>()) {
      out.baseHeight = tr->position.y;
    }
    return true;
  }
  return false;
}

/// @brief 指定 XZ の水面の高さを、描画と同じ式で求める
float SampleWaterHeight(Scene *scene, float x, float z) {
  RC::WaterWaveParams params;
  if (!BuildWaterParams(scene, params)) return 0.0f;

  RC::Vector4 raw[RC::WaterSurface::kMaxObstacles] = {};
  const int count =
      RC::GetWaterObstacles(raw, RC::WaterSurface::kMaxObstacles);
  RC::WaterObstacle obstacles[RC::WaterSurface::kMaxObstacles] = {};
  for (int i = 0; i < count; ++i) {
    obstacles[i].pos = {raw[i].x, raw[i].y, raw[i].z};
    obstacles[i].radius = raw[i].w;
  }

  return RC::WaterSurface::SampleHeight(
      params, RC::GetWaterTime(), x, z, obstacles, count,
      RC::GetWaterReflectStrength(), RC::GetWaterReflectRange());
}

/// @brief 浮力スクリプトが喫水の基準にしているのと同じ水面高さを求める
/// @details BuoyancyScript は useFourSamples が立っていると前後左右4点の平均を
///          基準にしている。ここで中心1点だけを見ると波高（0.3m 級）ぶん
///          ずれるため、「余分な沈み込みがほぼ 0」という 0.1m オーダーの主張を
///          測るには誤差が大きすぎる。同じ取り方に揃える。
/// @param yaw 対象の Y 軸回転（ラジアン）。サンプル点はこれに合わせて回す
float SampleWaterHeightLikeBuoyancy(Scene *scene, float x, float z, float yaw,
                                    const nlohmann::json &params) {
  const float fwd = params.value("sampleRadius", 1.0f);
  // 条件も BuoyancyScript と同じにする（useFourSamples && sampleRadius > 0）
  if (!params.value("useFourSamples", true) || fwd <= 0.0f) {
    return SampleWaterHeight(scene, x, z);
  }
  float side = params.value("sampleRadiusSide", 0.0f);
  if (side <= 0.0f) side = fwd;

  // サンプル点は世界軸ではなくエンティティの向きに合わせて回す。
  // 世界軸のまま取ると、船のように向きが変わる物体で前後と左右が入れ替わり、
  // sampleRadius と sampleRadiusSide が違う値のときに別の場所を測ることになる。
  const float cy = std::cos(yaw);
  const float sy = std::sin(yaw);
  const float fx = sy * fwd,  fz = cy * fwd;   // 船首方向
  const float rx = cy * side, rz = -sy * side; // 右舷方向

  return (SampleWaterHeight(scene, x + fx, z + fz) +
          SampleWaterHeight(scene, x - fx, z - fz) +
          SampleWaterHeight(scene, x + rx, z + rz) +
          SampleWaterHeight(scene, x - rx, z - rz)) *
         0.25f;
}

// ---------------------------------------------------------------------------
// ステップごとの毎フレーム処理
// ---------------------------------------------------------------------------
// 撮影モードは画面にゲーム画面しか出さない（字幕は録画の邪魔になるので廃止した）。
// 字幕と一緒に動いていた処理のうち、自動切り替えと計測だけをここに残してある。
// 計測結果はログに出す。

void TickPcf() {
  auto &dbg = RC::GetRenderContext().ShadowDebug();
  dbg.enabled = true; // このステップのあいだは上書きを有効にする

  if (g_autoToggle) {
    constexpr float kPeriod = 2.5f;
    g_toggleOn = std::fmod(g_stepTime, kPeriod * 2.0f) >= kPeriod;
  }
  dbg.pcfRadius = g_toggleOn ? 1.0f : 0.0f;
}

void TickReflect() {
  if (g_autoToggle) {
    constexpr float kPeriod = 3.0f;
    g_toggleOn = std::fmod(g_stepTime, kPeriod * 2.0f) >= kPeriod;
  }
  // 0 は「無効」ではなく既定値 1.0 へのフォールバックとして解釈されるため（D-14）、
  // OFF 側は 0 ではなく十分小さい正の値を使う。
  RC::SetWaterReflectOverride(true, g_toggleOn ? 1.0f : 0.001f,
                              RC::GetWaterReflectRange());
}

/// @brief 浮力の落下計測を進める（F7 で開始したあと、水面へ戻るまで追う）
void TickBuoyancy(Scene *scene, float deltaTime) {
  if (g_drop.phase != DropMeasure::Phase::Falling &&
      g_drop.phase != DropMeasure::Phase::Submerged) {
    return;
  }

  Entity *target = nullptr;
  for (auto *e : FindWithScript(scene, "BuoyancyScript")) {
    if (e->GetId() == g_drop.targetId) { target = e; break; }
  }
  auto *tr = target ? target->GetComponent<TransformComponent>() : nullptr;
  if (!tr) {
    g_drop.phase = DropMeasure::Phase::Idle; // 対象が消えた
    return;
  }

  const nlohmann::json &params = CachedSerialize(target, "BuoyancyScript");
  g_drop.halfHeight = params.value("halfHeight", 1.0f);
  const float waterY = SampleWaterHeightLikeBuoyancy(
      scene, tr->position.x, tr->position.z, tr->rotation.y, params);
  const float topY = tr->position.y + g_drop.halfHeight;

  switch (g_drop.phase) {
  case DropMeasure::Phase::Falling:
    g_drop.submergedTime += deltaTime;
    if (topY < waterY) {
      g_drop.phase = DropMeasure::Phase::Submerged;
      g_drop.everFullySubmerged = true;
      g_drop.submergedTime = 0.0f;
      g_drop.deepest = 0.0f;
    } else if (g_drop.submergedTime > 8.0f) {
      g_drop.phase = DropMeasure::Phase::Done; // 水面に届かなかった
    }
    break;
  case DropMeasure::Phase::Submerged:
    g_drop.submergedTime += deltaTime;
    g_drop.deepest = (std::max)(g_drop.deepest, waterY - topY);
    if (topY >= waterY || g_drop.submergedTime > 20.0f) {
      g_drop.recoverTime = g_drop.submergedTime; // 20 秒を超えたら「戻ってこない」
      g_drop.phase = DropMeasure::Phase::Done;
    }
    break;
  default:
    break;
  }

  if (g_drop.phase == DropMeasure::Phase::Done) {
    char buf[256];
    if (g_drop.everFullySubmerged) {
      std::snprintf(buf, sizeof(buf),
                    "[Capture] 浮力の落下計測: 落下 %.1f m / 余分な沈み込み %.2f m / "
                    "水面への復帰 %.2f 秒",
                    g_drop.startY - g_drop.waterY, g_drop.deepest, g_drop.recoverTime);
    } else {
      std::snprintf(buf, sizeof(buf),
                    "[Capture] 浮力の落下計測: 落下 %.1f m で一度も完全水没しなかった",
                    g_drop.startY - g_drop.waterY);
    }
    Log::Print(buf);
  }
}

void TickStep(Scene *scene, float deltaTime) {
  switch (kSteps[g_step].kind) {
  case Kind::Pcf:      TickPcf(); break;
  case Kind::Reflect:  TickReflect(); break;
  case Kind::Buoyancy: TickBuoyancy(scene, deltaTime); break;
  default: break;
  }
}

// ---------------------------------------------------------------------------
// F7 の実行
// ---------------------------------------------------------------------------

void RunStepAction(Scene *scene) {
  const Step &step = kSteps[g_step];
  switch (step.kind) {
  case Kind::Week1Scene:
    DebugBridge::RequestScene("CG4");
    break;

  case Kind::Pcf:
  case Kind::Reflect:
    g_autoToggle = !g_autoToggle;
    break;

  case Kind::SceneFlow: {
    const char *next = kSceneFlow[g_sceneFlowIndex % kSceneFlowCount];
    DebugBridge::RequestScene(next);
    g_sceneFlowIndex = (g_sceneFlowIndex + 1) % kSceneFlowCount;
    break;
  }

  case Kind::SpriteDigits: {
    auto &s = GameSession::Get();
    s.SetScore(1234567);
    s.SetPlayerHp(3, 5);
    s.SetWaypointProgress(6, 8);
    s.SetEnemiesDefeated(12);
    s.SetChestsCollected(3);
    s.SetDamageTaken(2);
    s.Finish(GameSession::Outcome::Cleared);
    DebugBridge::RequestScene("Result");
    break;
  }

  case Kind::Resources: {
    auto &ctx = RC::GetRenderContext();
    g_baseline.taken = true;
    g_baseline.spriteInUse = ctx.Sprites().InUseCount();
    g_baseline.pointLights = RC::GetActivePointLightCount();
    g_baseline.spotLights = RC::GetActiveSpotLightCount();
    g_baseline.areaLights = RC::GetActiveAreaLightCount();
    g_baseline.entities = scene ? scene->GetEntities().size() : 0u;
    Log::Print("[Capture] リソースの基準値を記録しました");
    break;
  }

  case Kind::Buoyancy: {
    auto floaters = FindWithScript(scene, "BuoyancyScript");
    Entity *target = nullptr;
    for (auto *e : floaters) {
      if (e->GetId() == g_drop.targetId) { target = e; break; }
    }
    if (!target && !floaters.empty()) target = floaters.front();
    if (!target) break;
    auto *tr = target->GetComponent<TransformComponent>();
    if (!tr) break;

    const nlohmann::json params =
        SafeSerialize(GetScript(target, "BuoyancyScript"));

    g_drop.targetId = target->GetId();
    g_drop.waterY = SampleWaterHeightLikeBuoyancy(
        scene, tr->position.x, tr->position.z, tr->rotation.y, params);
    // 前回の上下動の速度が残っていると同じ高さから落としても結果がばらつく。
    // BuoyancyScript の内部速度は型が見えないのでタグ経由で消してもらい、
    // Rigidbody 側はここで直接消す（タグ経由だと 1 フレーム遅れるため）。
    target->SetTag("reset_motion", 1);
    if (auto *rb = target->GetComponent<RigidbodyComponent>()) {
      rb->velocity = {0.0f, 0.0f, 0.0f};
    }
    tr->position.y = g_drop.waterY + g_drop.dropHeight;
    g_drop.startY = tr->position.y;
    g_drop.phase = DropMeasure::Phase::Falling;
    g_drop.submergedTime = 0.0f;
    g_drop.deepest = 0.0f;
    g_drop.recoverTime = 0.0f;
    g_drop.everFullySubmerged = false;
    Log::Print("[Capture] 浮力の落下計測を開始しました");
    break;
  }

  case Kind::Wave: {
    if (!scene) break;
    Entity *mgr = scene->FindEntityByName("WaveManager");
    if (!mgr) {
      auto found = FindWithScript(scene, "WaveManagerScript");
      if (!found.empty()) mgr = found.front();
    }
    if (!mgr) break;

    const int active = mgr->GetTagInt("wave_active", 0);
    const int pending = mgr->GetTagInt("wave_request", 0);

    // 進行中なら強制クリア。強制クリアは進行中のIDと一致していないと受理されない。
    if (active != 0) {
      mgr->SetTag("wave_force_clear", active);
      Log::Print("[Capture] ウェーブを強制クリアしました");
      break;
    }
    // Intro 中と Cleared 中は wave_active も wave_request も 0 に戻っていて、
    // 外からは Idle と見分けが付かない。ここで要求を通すと、クリア演出の途中で
    // 次のウェーブが始まってしまう（動画で一番見せたい場面が壊れる）。
    // WaveManager が公開している wave_busy で弾く。
    if (pending != 0 || mgr->GetTagInt("wave_busy", 0) != 0) break;

    // 要求するIDは「シーンに置いてあるスポナーの担当ウェーブのうち、まだクリアしていない最小値」。
    // waves_cleared + 1 にすると、スポナーが無いIDを要求したときに
    // WaveManager が即クリア扱いにするため、敵が湧かないまま累計だけ増える。
    const int lastCleared = mgr->GetTagInt("wave_cleared_id", 0);
    int next = 0;
    for (auto *s : FindWithScript(scene, "WaveSpawnerScript")) {
      const int id = s->GetTagInt("spawner_wave_id", 0);
      if (id <= 0 || id <= lastCleared) continue;
      if (next == 0 || id < next) next = id;
    }
    if (next == 0) {
      // 全部クリア済みなら最小のIDへ戻して撮り直せるようにする
      for (auto *s : FindWithScript(scene, "WaveSpawnerScript")) {
        const int id = s->GetTagInt("spawner_wave_id", 0);
        if (id <= 0) continue;
        if (next == 0 || id < next) next = id;
      }
    }
    if (next == 0) {
      Log::Print("[Capture] ウェーブスポナーが見つからないため開始できません");
      break;
    }
    mgr->SetTag("wave_request", next);
    Log::Print("[Capture] ウェーブ " + std::to_string(next) + " の開始を要求しました");
    break;
  }

  case Kind::Ship: {
    auto ships = FindWithScript(scene, "ShipEnemyScript");
    for (auto *e : ships) {
      if (e->GetTagInt("enemy_defeated", 0) != 0) continue;
      // pending_damage は 1 フレーム分をまとめて消費する加算式のタグ。
      const int hp = (std::max)(e->GetTagInt("current_hp", 1), 1);
      e->SetTag("pending_damage", e->GetTagInt("pending_damage", 0) + hp);
      Log::Print("[Capture] 船を撃沈させました");
      break;
    }
    break;
  }

  default:
    break;
  }
}

// ---------------------------------------------------------------------------
// ステップ移動
// ---------------------------------------------------------------------------

/// @brief ステップを離れるときに、そのステップが入れた上書きを畳む
void LeaveStep() {
  const Step &step = kSteps[g_step];
  if (step.kind == Kind::Pcf) {
    RC::GetRenderContext().ShadowDebug().enabled = false;
  } else if (step.kind == Kind::Reflect) {
    RC::SetWaterReflectOverride(false, 1.0f, 3.0f);
  }
}

void GoToStep(int index) {
  LeaveStep();
  g_step = (index % kStepCount + kStepCount) % kStepCount;
  g_stepTime = 0.0f;
  g_autoToggle = true;
  g_toggleOn = false;
}

#endif // RC_ENABLE_IMGUI

} // namespace

// ===========================================================================
// 公開関数
// ===========================================================================

bool CaptureMode::IsActive() { return g_active; }

bool CaptureMode::WantsPlaying() { return g_wantsPlaying; }

bool CaptureMode::IsGameHovered() { return g_gameHovered; }

void CaptureMode::SetActive(bool active) {
  if (g_active == active) return;
#if RC_ENABLE_IMGUI
  if (!active) {
    LeaveStep(); // 抜けるときは上書きを必ず畳む
  }
#endif
  g_active = active;
  g_wantsPlaying = active;
  // 入った直後の 1 フレームはまだ Draw を通っていないので、
  // ゲーム画面の上にいる前提にしておく（そうしないと初回だけ撃てない）。
  g_gameHovered = true;
  if (active) {
#if RC_ENABLE_IMGUI
    g_stepTime = 0.0f;
#endif
    Log::Print("[Capture] 撮影モードを開始しました（F10 次へ / F8 戻る / F7 実行 / F9 終了）");
  } else {
    Log::Print("[Capture] 撮影モードを終了しました");
  }
}

bool CaptureMode::HandleHotkeys() {
#if RC_ENABLE_IMGUI
  // ゲーム側が使っているキー（方向キー・スペース・F1/F3/F4）とぶつからないよう、
  // 撮影モードの操作は F7 〜 F10 に寄せてある。
  if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) {
    SetActive(!g_active);
    return true;
  }
#endif
  return false;
}

void CaptureMode::Draw(D3D12_GPU_DESCRIPTOR_HANDLE viewportSrv, Dx12Core *core,
                       Scene *currentScene, float deltaTime) {
#if RC_ENABLE_IMGUI
  if (!g_active) return;

  g_stepTime += deltaTime;
  g_clock += deltaTime;

  // --- キー操作 ---------------------------------------------------------
  if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) GoToStep(g_step + 1);
  if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) GoToStep(g_step - 1);
  if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) RunStepAction(currentScene);

  const ImGuiViewport *vp = ImGui::GetMainViewport();

  // --- ゲーム画面を全画面で敷く ----------------------------------------
  // アスペクト比を保って中央に置く（引き伸ばすと録画で見栄えが悪くなる）。
  {
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    // アスペクト比を保つと上下（または左右）に帯が出る。
    // エディタの背景色（濃いグレー）のままだと録画で目立つので黒にする。
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    // NoInputs にしてあるのは見た目のためではなく描画順のため。
    // ImGui は NoBringToFrontOnFocus 付きのウィンドウをリストの先頭へ入れる＝
    // 最初に描く＝最背面になるので、それを付けると DockSpace の不透明な背景に
    // ゲーム画面が隠れて真っ暗な動画しか撮れない。
    // かといって外すと、ゲーム画面をクリックした瞬間に前面へ来て字幕を隠す。
    // NoInputs ならホバー判定の対象外になり、順序が固定されたまま
    // マウス入力もゲーム側へ素通りする。
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##CaptureView", nullptr, flags)) {
      if (viewportSrv.ptr != 0 && vp->Size.x > 0.0f && vp->Size.y > 0.0f) {
        float aspect = 16.0f / 9.0f;
        if (core) {
          const auto &gameVp = core->Viewport();
          if (gameVp.Width > 0.0f && gameVp.Height > 0.0f) {
            aspect = gameVp.Width / gameVp.Height;
          }
        }
        float w = vp->Size.x;
        float h = w / aspect;
        if (h > vp->Size.y) {
          h = vp->Size.y;
          w = h * aspect;
        }
        ImGui::SetCursorPos(ImVec2((vp->Size.x - w) * 0.5f, (vp->Size.y - h) * 0.5f));
        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)viewportSrv.ptr, ImVec2(w, h));

        // ゲームへマウス座標を渡す。
        // 通常は Viewport パネルがこれをやっているが、撮影モードでは
        // そのパネルを描かないので、ここで同じことをしないと
        // 照準がマウスに追従しなくなる（画面に敷いた矩形が
        // レターボックスで中央寄せされているぶんも差し引く）。
        if (auto *input = Input::GetInstance()) {
          float gameW = w, gameH = h;
          if (core) {
            const auto &gameVp = core->Viewport();
            if (gameVp.Width > 0.0f && gameVp.Height > 0.0f) {
              gameW = gameVp.Width;
              gameH = gameVp.Height;
            }
          }
          const ImVec2 mouse = ImGui::GetMousePos();
          input->SetGameMousePosition(((mouse.x - imageMin.x) / w) * gameW,
                                      ((mouse.y - imageMin.y) / h) * gameH);
        }
      }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
  }

  // --- ステップの処理 ---------------------------------------------------
  // 画面に出すのはゲーム画面だけ。操作できる窓が無いので、
  // マウス入力は常にゲームへ通す。
  TickStep(currentScene, deltaTime);
  g_gameHovered = true;
#else
  (void)viewportSrv;
  (void)core;
  (void)currentScene;
  (void)deltaTime;
#endif
}
