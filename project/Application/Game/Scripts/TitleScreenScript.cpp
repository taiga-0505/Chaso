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
/// @brief タイトル画面（真上から見た水面 ＋ 水面に浮くタイトル文字 ＋ スタート／終了メニュー）
/// @details
///   構成:
///     - 水面と真上視点のカメラは Title.json 側に置く（WaterComponent / CameraComponent）。
///       このスクリプトは水面を「読む」だけで、生成はしない。
///     - タイトル文字は 1 文字ずつ TextMeshComponent を持つエンティティとして OnCreate で生成し、
///       毎フレーム RC::WaterSurface（Water.VS.hlsl と同じ Gerstner 式）で水面の高さと法線を
///       サンプルして、浮き沈み・傾きを追従させる。
///     - メニューは OnRender で RC::DrawString による 2D 描画。
///       ↑↓ / W S / 十字キー / 左スティックで選択、Space / Enter / A ボタンで決定。
///       「スタート」→ startScene へ遷移、「ゲーム終了」→ PostQuitMessage(0)。
///
///   注意:
///     - DataDrivenScene 側の「導線シーンで Space → 次のシーン」判定から Title を外してあること。
///       残っていると Space で Select へ飛んでしまい、メニューと二重判定になる。
///     - TextMesh の読める面は -Z を向くので、rotation.x = +π/2 で +Y（真上）へ向ける。
///       文字の上方向は +Z になる。カメラも rotation.x = +π/2（真下向き）なので画面上＝+Z で一致する。
///
///   JSON (scriptDataList) で設定できる項目:
///     "titleText"      : タイトル文字列（UTF-8、1 文字ずつ分解して浮かせる）
///     "fontPath"       : 文字メッシュのフォント
///     "letterSize"     : 1em の高さ（ワールド m）
///     "letterDepth"    : 押し出し厚さ（m）。半分が水面下に沈む
///     "letterColor"    : 文字色 RGBA
///     "outlineEnabled" / "outlineWidth" / "outlineColor" : 縁取り
///     "letterPositions": [[x, z, yaw], ...] 文字ごとの配置（足りない分は自動配置）
///     "floatOffset"    : 文字の底面を水面（足元でいちばん高い点）からどれだけ浮かせるか（m）
///     "bobAmplitude" / "bobPeriod" : ぷかぷか上下する量（m）と周期（秒）
///     "driftRadius" / "driftPeriod" : 文字がゆっくり漂う円運動の半径（m）と周期（秒）
///     "tiltScale"      : 水面法線への傾き追従の強さ（0 で傾かない）
///     "menuFontPath" / "menuFontSize" : メニューのフォント
///     "menuBottomMargin" : 画面下端からメニュー最下行までの距離（px）
///     "startScene"     : 「スタート」で遷移するシーン名
///     "quitEnabled"    : 「ゲーム終了」でアプリを終了するか（false ならログのみ）
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
  float tiltScale = 1.0f;

  // ---- メニュー ----
  std::string menuFontPath = "Resources/fonts/Kiwi_Maru/KiwiMaru-Regular.ttf";
  float menuFontSize = 40.0f;
  float menuBottomMargin = 90.0f;
  std::string startScene = "Game";
  bool quitEnabled = true;

  nlohmann::json Serialize() override {
    nlohmann::json positions = nlohmann::json::array();
    for (const auto &p : letterPositions) positions.push_back({p.x, p.y, p.z});
    return {
        {"titleText", titleText},
        {"fontPath", fontPath},
        {"letterSize", letterSize},
        {"letterDepth", letterDepth},
        {"letterColor", {letterColor.x, letterColor.y, letterColor.z, letterColor.w}},
        {"outlineEnabled", outlineEnabled},
        {"outlineWidth", outlineWidth},
        {"outlineColor", {outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w}},
        {"letterPositions", positions},
        {"floatOffset", floatOffset},
        {"bobAmplitude", bobAmplitude},
        {"bobPeriod", bobPeriod},
        {"driftRadius", driftRadius},
        {"driftPeriod", driftPeriod},
        {"tiltScale", tiltScale},
        {"menuFontPath", menuFontPath},
        {"menuFontSize", menuFontSize},
        {"menuBottomMargin", menuBottomMargin},
        {"startScene", startScene},
        {"quitEnabled", quitEnabled},
    };
  }

  void Deserialize(const nlohmann::json &j) override {
    auto readVec4 = [&](const char *key, RC::Vector4 &out) {
      if (!j.contains(key)) return;
      const auto &c = j[key];
      if (c.is_array() && c.size() >= 4) {
        out = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
      }
    };
    if (j.contains("titleText")) titleText = j["titleText"].get<std::string>();
    if (j.contains("fontPath")) fontPath = j["fontPath"].get<std::string>();
    if (j.contains("letterSize")) letterSize = j["letterSize"].get<float>();
    if (j.contains("letterDepth")) letterDepth = j["letterDepth"].get<float>();
    readVec4("letterColor", letterColor);
    if (j.contains("outlineEnabled")) outlineEnabled = j["outlineEnabled"].get<bool>();
    if (j.contains("outlineWidth")) outlineWidth = j["outlineWidth"].get<float>();
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
    if (j.contains("floatOffset")) floatOffset = j["floatOffset"].get<float>();
    if (j.contains("bobAmplitude")) bobAmplitude = j["bobAmplitude"].get<float>();
    if (j.contains("bobPeriod")) bobPeriod = j["bobPeriod"].get<float>();
    if (j.contains("driftRadius")) driftRadius = j["driftRadius"].get<float>();
    if (j.contains("driftPeriod")) driftPeriod = j["driftPeriod"].get<float>();
    if (j.contains("tiltScale")) tiltScale = j["tiltScale"].get<float>();
    if (j.contains("menuFontPath")) menuFontPath = j["menuFontPath"].get<std::string>();
    if (j.contains("menuFontSize")) menuFontSize = j["menuFontSize"].get<float>();
    if (j.contains("menuBottomMargin")) menuBottomMargin = j["menuBottomMargin"].get<float>();
    if (j.contains("startScene")) startScene = j["startScene"].get<std::string>();
    if (j.contains("quitEnabled")) quitEnabled = j["quitEnabled"].get<bool>();
  }

#if RC_ENABLE_IMGUI
  void OnImGui() override {
    ImGui::Text("Letters: %d  Selected: %s", static_cast<int>(letters_.size()),
                selected_ == kMenuStart ? "Start" : "Quit");
    ImGui::Text("Water time: %.2f", RC::GetWaterTime());
    ImGui::DragFloat("Float Offset", &floatOffset, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("Bob Amplitude", &bobAmplitude, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Bob Period", &bobPeriod, 0.1f, 0.2f, 20.0f);
    ImGui::DragFloat("Drift Radius", &driftRadius, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Drift Period", &driftPeriod, 0.1f, 0.5f, 60.0f);
    ImGui::DragFloat("Tilt Scale", &tiltScale, 0.05f, 0.0f, 3.0f);
    ImGui::Separator();
    ImGui::DragFloat("Menu Font Size", &menuFontSize, 1.0f, 8.0f, 128.0f);
    ImGui::DragFloat("Menu Bottom Margin", &menuBottomMargin, 1.0f, 0.0f, 600.0f);
    ImGui::Checkbox("Quit Enabled", &quitEnabled);
    ImGui::TextUnformatted(("Start scene: " + startScene).c_str());
  }
#endif

protected:
  void OnCreate() override {
    // 前のシーン（Game）の障害物が水面の定数バッファに残っていると、
    // タイトルの水面に岩の反射波が出てしまう。ここには障害物が無いので空にする。
    RC::SetWaterObstacles(nullptr, 0);

    menuFont_ = RC::LoadFont(menuFontPath, menuFontSize);
    if (menuFont_ < 0) {
      Log::Print("[TitleScreenScript] failed to load menu font: " + menuFontPath);
    }

    SpawnLetters();
  }

  void OnUpdate(float deltaTime) override {
    time_ += deltaTime;
    blinkTimer_ += deltaTime;

    UpdateLetters();

    if (!decided_) {
      HandleInput();
    }
  }

  void OnDestroy() override {
    // エディタの停止 → 再生でスクリプトだけ作り直されると文字が二重に生えるため、
    // 自分が生成した文字エンティティは自分で片付ける。
    for (auto &l : letters_) {
      if (auto e = l.entity.lock()) e->Destroy();
    }
    letters_.clear();

    if (menuFont_ >= 0) {
      RC::UnloadFont(menuFont_);
      menuFont_ = -1;
    }
  }

  void OnRender() override {
    float screenW = 1280.0f;
    float screenH = 720.0f;
    auto &rc = RC::GetRenderContext();
    if (rc.Ctx() && rc.Ctx()->app) {
      screenW = static_cast<float>(rc.Ctx()->app->width);
      screenH = static_cast<float>(rc.Ctx()->app->height);
    }
    DrawMenu(screenW, screenH);
  }

private:
  enum MenuItem { kMenuStart = 0, kMenuQuit = 1, kMenuCount = 2 };

  /// @brief 水面に浮かせる 1 文字ぶんの状態
  struct Letter {
    std::weak_ptr<Entity> entity;
    RC::Vector3 basePos{};   ///< 静水面上の基準位置（x, 0, z）
    float yaw = 0.0f;        ///< 水面上での向き（rad）
    float phase = 0.0f;      ///< 漂いの位相オフセット
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

  void SpawnLetters() {
    Scene *scene = GetScene();
    if (!scene) return;

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

      auto entity = scene->CreateEntity("TitleLetter_" + std::to_string(i));
      if (!entity) continue;

      auto &tr = entity->AddComponent<TransformComponent>();
      tr.position = {placement.x, floatOffset, placement.y};
      tr.rotation = {kHalfPi, placement.z, 0.0f};
      tr.scale = {1.0f, 1.0f, 1.0f};

      auto &tm = entity->AddComponent<TextMeshComponent>();
      tm.text = chars[i];
      tm.fontPath = fontPath;
      tm.size = letterSize;
      tm.depth = letterDepth;
      tm.align = TextAlign::Center;
      tm.color = letterColor;
      tm.lightingMode = -1; // DirectionalLight に追従
      tm.outlineEnabled = outlineEnabled;
      tm.outlineWidth = outlineWidth;
      tm.outlineColor = outlineColor;
      tm.outlineUnlit = true;

      // メッシュ生成（EnsureTextMesh）はここで済む
      scene->InitDynamicEntityRuntime(*entity);

      Letter l;
      l.entity = entity;
      l.basePos = {placement.x, 0.0f, placement.y};
      l.yaw = placement.z;
      l.phase = static_cast<float>(i) * 1.37f;
      letters_.push_back(l);
    }

    // 生成直後の 1 フレームだけ原点にメッシュが見えるのを防ぐため、
    // Transform をすぐ描画側へも書いておく
    UpdateLetters();

    Log::Print("[TitleScreenScript] spawned " + std::to_string(letters_.size()) +
               " letters for \"" + titleText + "\"");
  }

  // ------------------------------------------------------------------
  // 文字の水面追従
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

  void UpdateLetters() {
    RC::WaterWaveParams params;
    const bool hasWater = BuildWaterParams(params);
    const float waterTime = RC::GetWaterTime();

    const float omega = (driftPeriod > 0.01f) ? (kTwoPi / driftPeriod) : 0.0f;

    for (auto &l : letters_) {
      auto e = l.entity.lock();
      if (!e) continue;
      auto *tr = e->GetComponent<TransformComponent>();
      if (!tr) continue;

      // ゆっくり円を描いて漂う
      RC::Vector3 pos = l.basePos;
      if (driftRadius > 0.0f && omega > 0.0f) {
        pos.x += std::cos(time_ * omega + l.phase) * driftRadius;
        pos.z += std::sin(time_ * omega * 0.8f + l.phase) * driftRadius;
      }

      // 文字は 1 枚の板なのに水面は文字の幅（≒ 3m）の中でも上下する。
      // 中心 1 点だけで高さを決めると、波の山が文字の端にかかったときに
      // その部分が水に沈んで見える。文字の足元（中心＋四隅）をサンプルして
      // いちばん高い水面を基準にすれば、どの部分も水面より下には来ない。
      RC::WaterSample sample;
      sample.height = hasWater ? params.baseHeight : 0.0f;
      float topWater = sample.height;
      if (hasWater) {
        sample = RC::WaterSurface::Sample(params, waterTime, pos.x, pos.z);
        topWater = sample.height;
        const float hx = letterSize * kFootprintHalf;
        const float hz = letterSize * kFootprintHalf;
        const float cy = std::cos(l.yaw), sy = std::sin(l.yaw);
        static const float kCorners[4][2] = {{-1.f, -1.f}, {1.f, -1.f}, {-1.f, 1.f}, {1.f, 1.f}};
        for (const auto &c : kCorners) {
          // 文字の向き（yaw）に合わせて四隅を回す
          const float lx = c[0] * hx, lz = c[1] * hz;
          const float wx = pos.x + lx * cy + lz * sy;
          const float wz = pos.z - lx * sy + lz * cy;
          topWater = (std::max)(topWater,
                                RC::WaterSurface::SampleHeight(params, waterTime, wx, wz));
        }
      }

      // ぷかぷか：水面追従に加えて、文字ごとに位相をずらした小さな上下動を足す
      float bob = 0.0f;
      if (bobAmplitude > 0.0f && bobPeriod > 0.01f) {
        const float w = kTwoPi / bobPeriod;
        bob = (std::sin(time_ * w + l.phase) * 0.5f + 0.5f) * bobAmplitude;
      }

      // 文字の底面が「足元でいちばん高い水面」より floatOffset だけ上に来るよう置く。
      // メッシュは厚さの中心が原点なので、底面 = position.y - depth/2。
      tr->position = {pos.x, topWater + letterDepth * 0.5f + floatOffset + bob, pos.z};

      // 文字の上面法線を水面法線へ合わせる。
      // 回転は v·Rx·Ry·Rz の順に掛かる（MakeAffineMatrix）。Rx(π/2 + a) で上面が
      // (0, cos a, sin a) を向き、Rz(b) で (-cos a·sin b, cos a·cos b, sin a) になる。
      // これを水面法線 n に一致させると a = asin(n.z), b = asin(-n.x / cos a)。
      // 間に挟む Ry(yaw) は小角度なので誤差は無視できる。
      float a = 0.0f, b = 0.0f;
      if (tiltScale > 0.0f) {
        const RC::Vector3 n = sample.normal;
        a = std::asin(std::clamp(n.z, -1.0f, 1.0f));
        const float ca = (std::max)(std::cos(a), 1e-3f);
        b = std::asin(std::clamp(-n.x / ca, -1.0f, 1.0f));
        a *= tiltScale;
        b *= tiltScale;
      }
      tr->rotation = {kHalfPi + a, l.yaw, b};

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
      blinkTimer_ = 0.0f;
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
  // メニュー描画
  // ------------------------------------------------------------------

  void DrawMenu(float screenW, float screenH) {
    static const char *kLabels[kMenuCount] = {"スタート", "ゲーム終了"};

    const float lineH = (menuFont_ >= 0) ? RC::GetFontLineHeight(menuFont_) : menuFontSize * 1.3f;
    const float gap = lineH * 0.35f;
    const float itemH = lineH + gap;
    const float bottomY = screenH - menuBottomMargin;
    const float topY = bottomY - itemH * static_cast<float>(kMenuCount) + gap;
    const float centerX = screenW * 0.5f;

    // 水面の上でも読めるように半透明の帯を敷く
    const float bandPad = lineH * 0.6f;
    RC::DrawBox({centerX - screenW * 0.22f, topY - bandPad},
                {centerX + screenW * 0.22f, bottomY + bandPad * 0.5f},
                {0.02f, 0.05f, 0.10f, 0.45f});

    // 選択中の項目は 1.2 秒周期でゆっくり明滅させる
    const float cycle = 1.2f;
    const float phase = std::fmod(blinkTimer_, cycle) / cycle;
    const float pulse = 0.75f + 0.25f * std::sin(phase * kTwoPi);

    for (int i = 0; i < kMenuCount; ++i) {
      const bool sel = (i == selected_);
      const float y = topY + itemH * static_cast<float>(i);
      const RC::Vector4 color = sel ? RC::Vector4{1.0f, 0.96f, 0.80f, pulse}
                                    : RC::Vector4{0.80f, 0.86f, 0.92f, 0.55f};

      if (menuFont_ >= 0) {
        RC::DrawString(menuFont_, kLabels[i], {centerX, y}, color, 1.0f, TextAlign::Center);
      } else {
        // フォントが無いときの保険：バーだけ描く
        RC::DrawBox({centerX - 90.0f, y + lineH * 0.35f}, {centerX + 90.0f, y + lineH * 0.65f}, color);
      }

      if (sel) {
        // 左右に小さな三角形のカーソル
        float w = 220.0f;
        if (menuFont_ >= 0) w = RC::MeasureString(menuFont_, kLabels[i]).x;
        const float cy = y + lineH * 0.5f;
        const float s = lineH * 0.22f;
        const float lx = centerX - w * 0.5f - s * 2.2f;
        const float rx = centerX + w * 0.5f + s * 2.2f;
        RC::DrawTriangle({lx, cy - s}, {lx + s * 1.4f, cy}, {lx, cy + s}, color);
        RC::DrawTriangle({rx, cy - s}, {rx - s * 1.4f, cy}, {rx, cy + s}, color);
      }
    }

    // 操作ヒント
    if (menuFont_ >= 0) {
      RC::DrawString(menuFont_, "↑↓ 選択　　SPACE 決定", {centerX, bottomY + bandPad * 0.55f},
                     {0.85f, 0.90f, 0.95f, 0.55f}, 0.45f, TextAlign::Center);
    }
  }

  static constexpr float kHalfPi = 1.57079632679f;
  static constexpr float kTwoPi = 6.28318530718f;
  static constexpr float kFootprintHalf = 0.45f; ///< 水面サンプルの四隅までの距離（letterSize 比）
  static constexpr SHORT kStickThreshold = 16000;

  std::vector<Letter> letters_;
  int menuFont_ = -1;
  int selected_ = kMenuStart;
  int prevStickDir_ = 0;
  bool decided_ = false;
  float time_ = 0.0f;
  float blinkTimer_ = 0.0f;
};

REGISTER_SCRIPT(TitleScreenScript)
