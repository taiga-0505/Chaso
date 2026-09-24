#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/TextMeshComponent.h"
#include "ECS/WaterComponent.h"
#include "Common/Math/MathUtils.h"
#include "Common/Water/WaterSurface.h"
#include "Common/Log/Log.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "Engine/Render/RenderContext.h"
#include "Application/Framework/App.h"
#include "Scene.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

/// @class TitleScreenScript
/// @brief タイトル画面（真上から見た水面 ＋ 水面に浮くタイトル文字 ＋ 水面に浮くメニュー）
/// @details
///   構成:
///     - 水面と真上視点のカメラは Title.json 側に置く（WaterComponent / CameraComponent）。
///       このスクリプトは水面を「読む」だけで、生成はしない。
///     - タイトル文字は 1 文字ずつ、メニュー（スタート／ゲーム終了）は 1 項目ずつ、
///       TextMeshComponent を持つエンティティとして OnCreate で生成する。
///       どれも毎フレーム RC::WaterSurface（Water.VS.hlsl と同じ Gerstner 式）で水面の
///       高さと法線をサンプルし、浮き沈み・傾きを追従させる（Floater）。
///     - メニューの選択は色・大きさ・浮き上がりで示す。
///       ↑↓ / W S / 十字キー / 左スティックで選択、Space / Enter / A ボタンで決定。
///       「スタート」→ startScene へ遷移、「ゲーム終了」→ PostQuitMessage(0)。
///     - 操作ヒントだけは小さな 2D 文字（OnRender / RC::DrawString）。showHint で消せる。
///
///   注意:
///     - DataDrivenScene 側の「導線シーンで Space → 次のシーン」判定から Title を外してあること。
///       残っていると Space で Select へ飛んでしまい、メニューと二重判定になる。
///     - TextMesh の読める面は -Z を向くので、rotation.x = +π/2 で +Y（真上）へ向ける。
///       文字の上方向は +Z になる。カメラも rotation.x = +π/2（真下向き）なので画面上＝+Z で一致する。
///     - TextMesh 本体の色は DataDrivenScene が毎フレーム同期しない（再生成時のみ）ため、
///       選択色は Material へ直接書く（ApplyMainColor）。
///
///   JSON (scriptDataList) で設定できる項目:
///     [タイトル文字]
///     "titleText"      : タイトル文字列（UTF-8、1 文字ずつ分解して浮かせる）
///     "fontPath"       : 文字メッシュのフォント
///     "letterSize"     : 1em の高さ（ワールド m）
///     "letterDepth"    : 押し出し厚さ（m）
///     "letterColor"    : 文字色 RGBA
///     "outlineEnabled" / "outlineWidth" / "outlineColor" : 縁取り
///     "letterPositions": [[x, z, yaw], ...] 文字ごとの配置（足りない分は自動配置）
///     "floatOffset"    : 文字の底面を水面（足元でいちばん高い点）からどれだけ浮かせるか（m）
///     "bobAmplitude" / "bobPeriod" : ぷかぷか上下する量（m）と周期（秒）
///     "driftRadius" / "driftPeriod" : 文字がゆっくり漂う円運動の半径（m）と周期（秒）
///     "tiltScale"      : 水面法線への傾き追従の強さ（0 で傾かない）
///     [メニュー]
///     "menuFontPath"   : メニュー文字メッシュのフォント
///     "menuSize"       : メニューの 1em の高さ（m）
///     "menuDepth"      : メニューの押し出し厚さ（m）
///     "menuPositions"  : [[x, z], [x, z]] スタート／ゲーム終了のベースライン位置（画面上が +Z）
///     "menuColor" / "menuSelectedColor" / "menuOutlineColor" : 非選択色・選択色・縁取り色
///     "menuSelectedScale" : 選択中の拡大率
///     "menuSelectedLift"  : 選択中に追加で浮かせる量（m）
///     "menuBobAmplitude"  : メニューの上下動（m。タイトル文字より控えめにする）
///     "menuTiltScale"     : メニューの傾き追従の強さ
///     "startScene"     : 「スタート」で遷移するシーン名
///     "quitEnabled"    : 「ゲーム終了」でアプリを終了するか（false ならログのみ）
///     [ヒント]
///     "showHint" / "hintFontPath" / "hintFontSize" : 画面下の操作ヒント（2D）
class TitleScreenScript : public ScriptableEntity {
public:
  // ---- タイトル文字 ----
  std::string titleText = "水天の射手";
  std::string fontPath = "Resources/fonts/Kiwi_Maru/KiwiMaru-Medium.ttf";
  float letterSize = 3.0f;
  float letterDepth = 0.5f;
  RC::Vector4 letterColor = {0.97f, 0.98f, 1.0f, 1.0f};
  bool outlineEnabled = true;
  float outlineWidth = 0.04f;
  RC::Vector4 outlineColor = {0.05f, 0.12f, 0.25f, 1.0f};
  std::vector<RC::Vector3> letterPositions; ///< x, z, yaw(rad)
  float floatOffset = 0.12f;
  float bobAmplitude = 0.08f;
  float bobPeriod = 2.8f;
  float driftRadius = 0.35f;
  float driftPeriod = 9.0f;
  float tiltScale = 0.7f;

  // ---- メニュー（3D 文字）----
  std::string menuFontPath = "Resources/fonts/Kiwi_Maru/KiwiMaru-Medium.ttf";
  float menuSize = 1.5f;
  float menuDepth = 0.3f;
  std::vector<RC::Vector2> menuPositions;                    ///< x, z（ベースライン）
  RC::Vector4 menuColor = {0.62f, 0.72f, 0.82f, 1.0f};         ///< 非選択
  RC::Vector4 menuSelectedColor = {1.0f, 0.92f, 0.62f, 1.0f}; ///< 選択中
  RC::Vector4 menuOutlineColor = {0.04f, 0.10f, 0.22f, 1.0f};
  float menuSelectedScale = 1.18f;
  float menuSelectedLift = 0.12f;
  float menuBobAmplitude = 0.04f;
  float menuTiltScale = 0.5f;
  std::string startScene = "Game";
  bool quitEnabled = true;

  // ---- 操作ヒント（2D）----
  bool showHint = true;
  std::string hintFontPath = "Resources/fonts/Kiwi_Maru/KiwiMaru-Regular.ttf";
  float hintFontSize = 20.0f;

  nlohmann::json Serialize() override {
    nlohmann::json positions = nlohmann::json::array();
    for (const auto &p : letterPositions) positions.push_back({p.x, p.y, p.z});
    nlohmann::json menuPos = nlohmann::json::array();
    for (const auto &p : menuPositions) menuPos.push_back({p.x, p.y});
    auto v4 = [](const RC::Vector4 &c) { return nlohmann::json{c.x, c.y, c.z, c.w}; };
    return {
        {"titleText", titleText},
        {"fontPath", fontPath},
        {"letterSize", letterSize},
        {"letterDepth", letterDepth},
        {"letterColor", v4(letterColor)},
        {"outlineEnabled", outlineEnabled},
        {"outlineWidth", outlineWidth},
        {"outlineColor", v4(outlineColor)},
        {"letterPositions", positions},
        {"floatOffset", floatOffset},
        {"bobAmplitude", bobAmplitude},
        {"bobPeriod", bobPeriod},
        {"driftRadius", driftRadius},
        {"driftPeriod", driftPeriod},
        {"tiltScale", tiltScale},
        {"menuFontPath", menuFontPath},
        {"menuSize", menuSize},
        {"menuDepth", menuDepth},
        {"menuPositions", menuPos},
        {"menuColor", v4(menuColor)},
        {"menuSelectedColor", v4(menuSelectedColor)},
        {"menuOutlineColor", v4(menuOutlineColor)},
        {"menuSelectedScale", menuSelectedScale},
        {"menuSelectedLift", menuSelectedLift},
        {"menuBobAmplitude", menuBobAmplitude},
        {"menuTiltScale", menuTiltScale},
        {"startScene", startScene},
        {"quitEnabled", quitEnabled},
        {"showHint", showHint},
        {"hintFontPath", hintFontPath},
        {"hintFontSize", hintFontSize},
    };
  }

  void Deserialize(const nlohmann::json &j) override {
    auto readF = [&](const char *key, float &out) {
      if (j.contains(key) && j[key].is_number()) out = j[key].get<float>();
    };
    auto readB = [&](const char *key, bool &out) {
      if (j.contains(key) && j[key].is_boolean()) out = j[key].get<bool>();
    };
    auto readS = [&](const char *key, std::string &out) {
      if (j.contains(key) && j[key].is_string()) out = j[key].get<std::string>();
    };
    auto readVec4 = [&](const char *key, RC::Vector4 &out) {
      if (!j.contains(key)) return;
      const auto &c = j[key];
      if (c.is_array() && c.size() >= 4) {
        out = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
      }
    };

    readS("titleText", titleText);
    readS("fontPath", fontPath);
    readF("letterSize", letterSize);
    readF("letterDepth", letterDepth);
    readVec4("letterColor", letterColor);
    readB("outlineEnabled", outlineEnabled);
    readF("outlineWidth", outlineWidth);
    readVec4("outlineColor", outlineColor);
    if (j.contains("letterPositions") && j["letterPositions"].is_array()) {
      letterPositions.clear();
      for (const auto &p : j["letterPositions"]) {
        if (!p.is_array() || p.size() < 2) continue;
        RC::Vector3 v{p[0].get<float>(), p[1].get<float>(), 0.0f};
        if (p.size() >= 3) v.z = p[2].get<float>();
        letterPositions.push_back(v);
      }
    }
    readF("floatOffset", floatOffset);
    readF("bobAmplitude", bobAmplitude);
    readF("bobPeriod", bobPeriod);
    readF("driftRadius", driftRadius);
    readF("driftPeriod", driftPeriod);
    readF("tiltScale", tiltScale);

    readS("menuFontPath", menuFontPath);
    readF("menuSize", menuSize);
    readF("menuDepth", menuDepth);
    if (j.contains("menuPositions") && j["menuPositions"].is_array()) {
      menuPositions.clear();
      for (const auto &p : j["menuPositions"]) {
        if (!p.is_array() || p.size() < 2) continue;
        menuPositions.push_back({p[0].get<float>(), p[1].get<float>()});
      }
    }
    readVec4("menuColor", menuColor);
    readVec4("menuSelectedColor", menuSelectedColor);
    readVec4("menuOutlineColor", menuOutlineColor);
    readF("menuSelectedScale", menuSelectedScale);
    readF("menuSelectedLift", menuSelectedLift);
    readF("menuBobAmplitude", menuBobAmplitude);
    readF("menuTiltScale", menuTiltScale);
    readS("startScene", startScene);
    readB("quitEnabled", quitEnabled);

    readB("showHint", showHint);
    readS("hintFontPath", hintFontPath);
    readF("hintFontSize", hintFontSize);
  }

#if RC_ENABLE_IMGUI
  void OnImGui() override {
    ImGui::Text("Letters: %d  Menu: %d  Selected: %s", static_cast<int>(letters_.size()),
                static_cast<int>(menu_.size()), selected_ == kMenuStart ? "Start" : "Quit");
    ImGui::Text("Water time: %.2f", RC::GetWaterTime());
    ImGui::SeparatorText("Title Letters");
    ImGui::DragFloat("Float Offset", &floatOffset, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("Bob Amplitude", &bobAmplitude, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Bob Period", &bobPeriod, 0.1f, 0.2f, 20.0f);
    ImGui::DragFloat("Drift Radius", &driftRadius, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Drift Period", &driftPeriod, 0.1f, 0.5f, 60.0f);
    ImGui::DragFloat("Tilt Scale", &tiltScale, 0.05f, 0.0f, 3.0f);
    ImGui::SeparatorText("Menu");
    ImGui::ColorEdit4("Menu Color", &menuColor.x);
    ImGui::ColorEdit4("Selected Color", &menuSelectedColor.x);
    ImGui::DragFloat("Selected Scale", &menuSelectedScale, 0.01f, 1.0f, 2.0f);
    ImGui::DragFloat("Selected Lift", &menuSelectedLift, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Menu Bob", &menuBobAmplitude, 0.01f, 0.0f, 0.5f);
    ImGui::DragFloat("Menu Tilt", &menuTiltScale, 0.05f, 0.0f, 2.0f);
    ImGui::Checkbox("Quit Enabled", &quitEnabled);
    ImGui::Checkbox("Show Hint", &showHint);
    ImGui::TextUnformatted(("Start scene: " + startScene).c_str());
    ImGui::TextDisabled("menuSize / menuPositions / fonts are applied on scene reload");
  }
#endif

protected:
  void OnCreate() override {
    // 前のシーン（Game）の障害物が水面の定数バッファに残っていると、
    // タイトルの水面に岩の反射波が出てしまう。ここには障害物が無いので空にする。
    RC::SetWaterObstacles(nullptr, 0);

    if (showHint) {
      hintFont_ = RC::LoadFont(hintFontPath, hintFontSize);
      if (hintFont_ < 0) {
        Log::Print("[TitleScreenScript] failed to load hint font: " + hintFontPath);
      }
    }

    SpawnLetters();
    SpawnMenu();

    // 生成直後の 1 フレームだけ原点にメッシュが見えるのを防ぐため、
    // Transform をすぐ描画側へも書いておく
    UpdateFloaters(0.0f);
  }

  void OnUpdate(float deltaTime) override {
    time_ += deltaTime;
    pulseTimer_ += deltaTime;

    UpdateFloaters(deltaTime);

    if (!decided_) {
      HandleInput();
    }
  }

  void OnDestroy() override {
    // エディタの停止 → 再生でスクリプトだけ作り直されると文字が二重に生えるため、
    // 自分が生成した文字エンティティは自分で片付ける。
    for (auto &f : letters_) {
      if (auto e = f.entity.lock()) e->Destroy();
    }
    for (auto &f : menu_) {
      if (auto e = f.entity.lock()) e->Destroy();
    }
    letters_.clear();
    menu_.clear();

    if (hintFont_ >= 0) {
      RC::UnloadFont(hintFont_);
      hintFont_ = -1;
    }
  }

  void OnRender() override {
    if (!showHint || hintFont_ < 0) return;

    float screenW = 1280.0f;
    float screenH = 720.0f;
    auto &rc = RC::GetRenderContext();
    if (rc.Ctx() && rc.Ctx()->app) {
      screenW = static_cast<float>(rc.Ctx()->app->width);
      screenH = static_cast<float>(rc.Ctx()->app->height);
    }
    DrawHint(screenW, screenH);
  }

private:
  enum MenuItem { kMenuStart = 0, kMenuQuit = 1, kMenuCount = 2 };

  /// @brief 水面に浮かせる 3D 文字 1 つぶんの状態（タイトル文字・メニュー項目で共通）
  struct Floater {
    std::weak_ptr<Entity> entity;
    RC::Vector3 basePos{};      ///< 静水面上の基準位置（x, 0, z）。z はベースライン
    float yaw = 0.0f;           ///< 水面上での向き（rad）
    float phase = 0.0f;         ///< 漂い・上下動の位相オフセット
    float depth = 0.2f;         ///< 押し出し厚さ（底面の位置を出すのに使う）
    float halfW = 1.0f;         ///< 足元サンプル範囲：ローカル X の半幅（scale 1 のとき）
    float zMin = 0.0f;          ///< 足元サンプル範囲：ローカル Y（＝ワールド Z）の下端
    float zMax = 1.0f;          ///< 足元サンプル範囲：ローカル Y（＝ワールド Z）の上端
    float drift = 0.0f;         ///< 円運動の半径（0 で漂わない）
    float bob = 0.0f;           ///< 上下動の振幅
    float tilt = 1.0f;          ///< 水面法線への追従の強さ
    float lift = 0.0f;          ///< 追加で浮かせる量（選択中のメニュー）
    float scale = 1.0f;         ///< 現在の拡大率（アニメーション）
    float targetScale = 1.0f;   ///< 目標の拡大率
  };

  // ------------------------------------------------------------------
  // 文字の生成
  // ------------------------------------------------------------------

  /// @brief UTF-8 文字列を 1 文字ずつに分解する
  static std::vector<std::string> SplitUtf8(const std::string &s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
      const unsigned char c = static_cast<unsigned char>(s[i]);
      size_t len = 1;
      if ((c & 0xF8) == 0xF0) len = 4;
      else if ((c & 0xF0) == 0xE0) len = 3;
      else if ((c & 0xE0) == 0xC0) len = 2;
      len = (std::min)(len, s.size() - i);
      std::string ch = s.substr(i, len);
      // 空白は浮かせない（詰めてしまう）
      if (ch != " " && ch != "　") out.push_back(ch);
      i += len;
    }
    return out;
  }

  /// @brief 配置指定が無い文字の既定位置
  /// @details 左から右へ読み順に並べつつ、Z（画面の上下）をばらつかせて
  ///          「バラバラに浮いている」感じにする。画面上が +Z。
  RC::Vector3 DefaultLetterPosition(int index, int count) const {
    const float spacing = letterSize * 1.55f;
    const float x0 = -spacing * static_cast<float>(count - 1) * 0.5f;
    // 決め打ちのジグザグ（乱数だと毎回位置が変わって見た目を詰められないため）
    static const float kZOffsets[] = {0.9f, -0.5f, 1.4f, -0.9f, 0.4f, -1.2f, 0.7f, -0.3f};
    static const float kYaws[] = {-0.10f, 0.08f, -0.04f, 0.12f, -0.07f, 0.05f, -0.11f, 0.06f};
    const int n = static_cast<int>(sizeof(kZOffsets) / sizeof(kZOffsets[0]));
    RC::Vector3 p{};
    p.x = x0 + spacing * static_cast<float>(index);
    p.y = letterSize * 0.9f + kZOffsets[index % n] * letterSize * 0.35f; // ここでは y に Z を仮置き
    p.z = kYaws[index % n];                                              // ここでは z に yaw を仮置き
    return p;
  }

  /// @brief 3D 文字エンティティを 1 つ生成して水面用の Floater として登録する
  /// @return 生成した Floater（エンティティが作れなければ entity が空）
  Floater CreateTextFloater(const std::string &name, const std::string &text,
                            const std::string &font, float size, float depth,
                            const RC::Vector4 &color, const RC::Vector4 &outline,
                            float outlineW, float x, float z, float yaw) {
    Floater f;
    f.basePos = {x, 0.0f, z};
    f.yaw = yaw;
    f.depth = depth;
    // AABB が取れなかったときの保険（全角 1 文字 ≒ 1em 幅、高さ 0.8em）
    f.halfW = size * 0.5f * static_cast<float>((std::max)(SplitUtf8(text).size(), size_t(1)));
    f.zMin = -size * 0.1f;
    f.zMax = size * 0.8f;

    Scene *scene = GetScene();
    if (!scene) return f;
    auto entity = scene->CreateEntity(name);
    if (!entity) return f;

    auto &tr = entity->AddComponent<TransformComponent>();
    tr.position = {x, depth * 0.5f + floatOffset, z};
    tr.rotation = {kHalfPi, yaw, 0.0f};
    tr.scale = {1.0f, 1.0f, 1.0f};

    auto &tm = entity->AddComponent<TextMeshComponent>();
    tm.text = text;
    tm.fontPath = font;
    tm.size = size;
    tm.depth = depth;
    tm.align = TextAlign::Center;
    tm.color = color;
    tm.lightingMode = -1; // DirectionalLight に追従
    tm.outlineEnabled = outlineEnabled;
    tm.outlineWidth = outlineW;
    tm.outlineColor = outline;
    tm.outlineUnlit = true;

    // メッシュ生成（EnsureTextMesh）はここで済む
    scene->InitDynamicEntityRuntime(*entity);

    // 生成されたメッシュの実寸で足元のサンプル範囲を決める。
    // ローカル X が幅、ローカル Y が文字の高さ（回転後はワールド Z）。
    if (tm.HasMesh() && tm.info.vertexCount > 0) {
      f.halfW = (std::max)(std::fabs(tm.info.min.x), std::fabs(tm.info.max.x));
      f.zMin = tm.info.min.y;
      f.zMax = tm.info.max.y;
    }

    f.entity = entity;
    return f;
  }

  void SpawnLetters() {
    const std::vector<std::string> chars = SplitUtf8(titleText);
    const int count = static_cast<int>(chars.size());
    letters_.clear();
    letters_.reserve(chars.size());

    for (int i = 0; i < count; ++i) {
      RC::Vector3 placement{}; // x, z, yaw
      if (i < static_cast<int>(letterPositions.size())) {
        placement = letterPositions[i];
      } else {
        const RC::Vector3 d = DefaultLetterPosition(i, count);
        placement = {d.x, d.y, d.z};
      }

      Floater f = CreateTextFloater("TitleLetter_" + std::to_string(i), chars[i], fontPath,
                                    letterSize, letterDepth, letterColor, outlineColor,
                                    outlineWidth, placement.x, placement.y, placement.z);
      if (f.entity.expired()) continue;
      f.phase = static_cast<float>(i) * 1.37f;
      f.drift = driftRadius;
      f.bob = bobAmplitude;
      f.tilt = tiltScale;
      letters_.push_back(f);
    }

    Log::Print("[TitleScreenScript] spawned " + std::to_string(letters_.size()) +
               " letters for \"" + titleText + "\"");
  }

  void SpawnMenu() {
    static const char *kLabels[kMenuCount] = {"スタート", "ゲーム終了"};
    static const char *kNames[kMenuCount] = {"TitleMenu_Start", "TitleMenu_Quit"};

    menu_.clear();
    menu_.reserve(kMenuCount);
    for (int i = 0; i < kMenuCount; ++i) {
      // 既定：画面中央の下寄りに 2 行。行間は 1.7em
      RC::Vector2 pos{0.0f, -menuSize * (2.6f + 1.7f * static_cast<float>(i))};
      if (i < static_cast<int>(menuPositions.size())) pos = menuPositions[i];

      const bool sel = (i == selected_);
      Floater f = CreateTextFloater(kNames[i], kLabels[i], menuFontPath, menuSize, menuDepth,
                                    sel ? menuSelectedColor : menuColor, menuOutlineColor,
                                    outlineWidth, pos.x, pos.y, 0.0f);
      if (f.entity.expired()) continue;
      f.phase = 2.0f + static_cast<float>(i) * 1.9f;
      f.drift = 0.0f; // メニューは読ませたいので漂わせない
      f.bob = menuBobAmplitude;
      f.tilt = menuTiltScale;
      f.scale = f.targetScale = sel ? menuSelectedScale : 1.0f;
      f.lift = sel ? menuSelectedLift : 0.0f;
      menu_.push_back(f);
    }
  }

  // ------------------------------------------------------------------
  // 水面追従
  // ------------------------------------------------------------------

  /// @brief シーン内の WaterComponent から波パラメータを組み立てる
  bool BuildWaterParams(RC::WaterWaveParams &out) const {
    Scene *scene = GetScene();
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
      if (auto *tr = e->GetComponent<TransformComponent>()) out.baseHeight = tr->position.y;
      return true;
    }
    return false;
  }

  void UpdateFloaters(float deltaTime) {
    RC::WaterWaveParams params;
    const bool hasWater = BuildWaterParams(params);
    const float waterTime = RC::GetWaterTime();

    for (auto &f : letters_) UpdateFloater(f, params, hasWater, waterTime, deltaTime);

    // メニューは選択状態で色・大きさ・浮き上がりを変える
    const float pulse = 0.5f + 0.5f * std::sin(pulseTimer_ * (kTwoPi / kPulsePeriod));
    for (size_t i = 0; i < menu_.size(); ++i) {
      Floater &f = menu_[i];
      const bool sel = (static_cast<int>(i) == selected_);
      f.targetScale = sel ? menuSelectedScale : 1.0f;
      f.lift = sel ? menuSelectedLift : 0.0f;

      RC::Vector4 color = menuColor;
      if (sel) {
        // 選択色 ⇔ 白 のあいだでゆっくり明滅
        const float t = pulse * 0.45f;
        color = {menuSelectedColor.x + (1.0f - menuSelectedColor.x) * t,
                 menuSelectedColor.y + (1.0f - menuSelectedColor.y) * t,
                 menuSelectedColor.z + (1.0f - menuSelectedColor.z) * t,
                 menuSelectedColor.w};
      }
      ApplyMainColor(f, color);
      UpdateFloater(f, params, hasWater, waterTime, deltaTime);
    }
  }

  /// @brief TextMesh 本体の色を書き換える
  /// @details DataDrivenScene は本体色を再生成時にしか Material へ反映しないので、
  ///          tm.color と Material の両方へ書く（再生成が起きても同じ色に戻るように）。
  static void ApplyMainColor(Floater &f, const RC::Vector4 &color) {
    auto e = f.entity.lock();
    if (!e) return;
    auto *tm = e->GetComponent<TextMeshComponent>();
    if (!tm) return;
    tm->color = color;
    if (tm->HasMesh()) {
      if (auto *mat = RC::GetPrimitiveMeshMaterialPtr(tm->meshHandle)) mat->color = color;
    }
  }

  void UpdateFloater(Floater &f, const RC::WaterWaveParams &params, bool hasWater,
                     float waterTime, float deltaTime) {
    auto e = f.entity.lock();
    if (!e) return;
    auto *tr = e->GetComponent<TransformComponent>();
    if (!tr) return;

    // 拡大率は目標へなめらかに寄せる（選択の切り替えでポンと弾む感じ）
    if (deltaTime > 0.0f) {
      const float k = (std::min)(1.0f, deltaTime * 12.0f);
      f.scale += (f.targetScale - f.scale) * k;
    } else {
      f.scale = f.targetScale;
    }

    // ゆっくり円を描いて漂う
    RC::Vector3 pos = f.basePos;
    if (f.drift > 0.0f && driftPeriod > 0.01f) {
      const float omega = kTwoPi / driftPeriod;
      pos.x += std::cos(time_ * omega + f.phase) * f.drift;
      pos.z += std::sin(time_ * omega * 0.8f + f.phase) * f.drift;
    }

    // 文字は 1 枚の板なのに水面は文字の幅の中でも上下する。
    // 中心 1 点だけで高さを決めると、波の山が文字の端にかかったときに
    // その部分が水に沈んで見える。文字の足元（幅方向に数点 × 上下端）をサンプルして
    // いちばん高い水面を基準にすれば、どの部分も水面より下には来ない。
    RC::WaterSample sample;
    sample.height = hasWater ? params.baseHeight : 0.0f;
    float topWater = sample.height;
    if (hasWater) {
      const float zc = (f.zMin + f.zMax) * 0.5f * f.scale; // 文字の見た目の中心（ベースラインからのずれ）
      sample = RC::WaterSurface::Sample(params, waterTime, pos.x, pos.z + zc);
      topWater = sample.height;

      const float hw = f.halfW * f.scale;
      const float cy = std::cos(f.yaw), sy = std::sin(f.yaw);
      // 幅が広い（メニューの 5 文字など）ほど横方向のサンプル数を増やす
      const int nx = std::clamp(static_cast<int>(hw / 1.2f) + 2, 2, 7);
      const float zs[2] = {f.zMin * f.scale, f.zMax * f.scale};
      for (int ix = 0; ix < nx; ++ix) {
        const float lx = -hw + 2.0f * hw * static_cast<float>(ix) / static_cast<float>(nx - 1);
        for (float lz : zs) {
          // 文字の向き（yaw）に合わせて回す
          const float wx = pos.x + lx * cy + lz * sy;
          const float wz = pos.z - lx * sy + lz * cy;
          topWater = (std::max)(topWater,
                                RC::WaterSurface::SampleHeight(params, waterTime, wx, wz));
        }
      }
    }

    // ぷかぷか：水面追従に加えて、位相をずらした小さな上下動を足す
    float bob = 0.0f;
    if (f.bob > 0.0f && bobPeriod > 0.01f) {
      const float w = kTwoPi / bobPeriod;
      bob = (std::sin(time_ * w + f.phase) * 0.5f + 0.5f) * f.bob;
    }

    // 底面が「足元でいちばん高い水面」より floatOffset (+lift) だけ上に来るよう置く。
    // メッシュは厚さの中心が原点なので、底面 = position.y - depth*scale/2。
    tr->position = {pos.x, topWater + f.depth * f.scale * 0.5f + floatOffset + f.lift + bob, pos.z};

    // 文字の上面法線を水面法線へ合わせる。
    // 回転は v·Rx·Ry·Rz の順に掛かる（MakeAffineMatrix）。Rx(π/2 + a) で上面が
    // (0, cos a, sin a) を向き、Rz(b) で (-cos a·sin b, cos a·cos b, sin a) になる。
    // これを水面法線 n に一致させると a = asin(n.z), b = asin(-n.x / cos a)。
    // 間に挟む Ry(yaw) は小角度なので誤差は無視できる。
    float a = 0.0f, b = 0.0f;
    if (f.tilt > 0.0f) {
      const RC::Vector3 n = sample.normal;
      a = std::asin(std::clamp(n.z, -1.0f, 1.0f));
      const float ca = (std::max)(std::cos(a), 1e-3f);
      b = std::asin(std::clamp(-n.x / ca, -1.0f, 1.0f));
      a *= f.tilt;
      b *= f.tilt;
    }
    tr->rotation = {kHalfPi + a, f.yaw, b};
    tr->scale = {f.scale, f.scale, f.scale};

    // 描画側の Transform も同期しておく（通常は次の Update 冒頭で同期される）
    if (auto *tm = e->GetComponent<TextMeshComponent>()) {
      if (tm->HasMesh()) {
        if (auto *p = RC::GetPrimitiveMeshTransformPtr(tm->meshHandle)) *p = tr->ToTransform();
      }
      if (tm->HasOutlineMesh()) {
        if (auto *p = RC::GetPrimitiveMeshTransformPtr(tm->outlineMeshHandle)) *p = tr->ToTransform();
      }
    }
  }

  // ------------------------------------------------------------------
  // 入力
  // ------------------------------------------------------------------

  void HandleInput() {
    SceneContext *ctx = GetSceneContext();
    if (!ctx || !ctx->input) return;
    Input *in = ctx->input;

    // 上下（キーボード / 十字キー / 左スティックのエッジ）
    int move = 0;
    if (in->IsKeyTrigger(DIK_UP) || in->IsKeyTrigger(DIK_W)) move -= 1;
    if (in->IsKeyTrigger(DIK_DOWN) || in->IsKeyTrigger(DIK_S)) move += 1;
    if (in->IsXInputConnected()) {
      if (in->IsXInputButtonTrigger(XINPUT_GAMEPAD_DPAD_UP)) move -= 1;
      if (in->IsXInputButtonTrigger(XINPUT_GAMEPAD_DPAD_DOWN)) move += 1;
      const SHORT ly = in->GetXInputThumbLY();
      int stickDir = 0;
      if (ly > kStickThreshold) stickDir = -1;       // 上
      else if (ly < -kStickThreshold) stickDir = 1;  // 下
      if (stickDir != 0 && stickDir != prevStickDir_) move += stickDir;
      prevStickDir_ = stickDir;
    }
    if (move != 0) {
      selected_ = (selected_ + move + kMenuCount) % kMenuCount;
      pulseTimer_ = 0.0f;
      // 切り替えた瞬間だけ少し大きくして「跳ねる」感じを出す
      if (selected_ >= 0 && selected_ < static_cast<int>(menu_.size())) {
        menu_[selected_].scale = menuSelectedScale * 1.12f;
      }
    }

    // 決定
    bool confirm = in->IsKeyTrigger(DIK_SPACE) || in->IsKeyTrigger(DIK_RETURN);
    if (in->IsXInputConnected() && in->IsXInputButtonTrigger(XINPUT_GAMEPAD_A)) confirm = true;
    if (!confirm) return;

    if (selected_ == kMenuStart) {
      if (RequestSceneChange(startScene)) {
        decided_ = true;
        Log::Print("[TitleScreenScript] start -> " + startScene);
      } else {
        Log::Print("[TitleScreenScript] scene change refused: " + startScene);
      }
    } else {
      if (quitEnabled) {
        Log::Print("[TitleScreenScript] quit requested");
        decided_ = true;
        // App::Run のメッセージループは WM_QUIT で抜けて Term() へ進む
        PostQuitMessage(0);
      } else {
        Log::Print("[TitleScreenScript] quit is disabled (quitEnabled = false)");
      }
    }
  }

  // ------------------------------------------------------------------
  // 操作ヒント（2D）
  // ------------------------------------------------------------------

  void DrawHint(float screenW, float screenH) {
    const char *text = "↑↓ 選択　　SPACE 決定";
    const float lineH = RC::GetFontLineHeight(hintFont_);
    const RC::Vector2 pos{screenW * 0.5f, screenH - lineH - 28.0f};
    // 水面の上でも読めるように、影を 1px ずらして先に描く
    RC::DrawString(hintFont_, text, {pos.x + 1.0f, pos.y + 1.0f}, {0.0f, 0.03f, 0.08f, 0.6f},
                   1.0f, TextAlign::Center);
    RC::DrawString(hintFont_, text, pos, {0.90f, 0.94f, 0.98f, 0.75f}, 1.0f, TextAlign::Center);
  }

  static constexpr float kHalfPi = 1.57079632679f;
  static constexpr float kTwoPi = 6.28318530718f;
  static constexpr float kPulsePeriod = 1.4f; ///< 選択中メニューの明滅周期（秒）
  static constexpr SHORT kStickThreshold = 16000;

  std::vector<Floater> letters_;
  std::vector<Floater> menu_;
  int hintFont_ = -1;
  int selected_ = kMenuStart;
  int prevStickDir_ = 0;
  bool decided_ = false;
  float time_ = 0.0f;
  float pulseTimer_ = 0.0f;
};

REGISTER_SCRIPT(TitleScreenScript)
