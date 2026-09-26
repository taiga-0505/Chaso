#pragma once
#include "Common/EngineConfig.h"
#include "Common/Log/Log.h"
#include "ECS/CameraComponent.h"
#include "ECS/Entity.h"
#include "ECS/GPUParticleComponent.h"
#include "ECS/TransformComponent.h"
#include "ECS/WaterComponent.h"
#include "Engine/Render/RenderContext.h"
#include "Graphics/PostProcess/PostProcess.h"
#include "Particle/GPUParticle.h"
#include "RenderCommon.h"
#include "Scene.h"
#include "Application/Game/Framework/UnderwaterLook.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <memory>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

/// @brief 真上視点のカメラを「水へ飛び込ませる」「深海から浮上させる」共通処理
/// @details Title（スタート時の飛び込み / 入場時の浮上）と Result（もう一度・タイトルへ の
///          飛び込み）が同じ動きを使う。シーン遷移の要求はスクリプトの持ち物
///          （ScriptableEntity::RequestSceneChange）なので、ここではカメラと画面効果だけを扱い、
///          「暗くなりきった（IsDone）」ことを呼び出し側へ知らせる。
///
///          水中の色は UnderwaterLook（Game と共通）で決めるので、飛び込みで暗くなりきった画と
///          次のシーンの浮上開始の画がつながる。
namespace WaterCameraFx {

// ------------------------------------------------------------------
// 小物
// ------------------------------------------------------------------

inline float Smooth01(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
inline float EaseOutCubic(float u) {
  u = std::clamp(u, 0.0f, 1.0f);
  const float v = 1.0f - u;
  return 1.0f - v * v * v;
}
inline float EaseOutQuad(float u) {
  u = std::clamp(u, 0.0f, 1.0f);
  return 1.0f - (1.0f - u) * (1.0f - u);
}
inline float EaseInQuad(float u) {
  u = std::clamp(u, 0.0f, 1.0f);
  return u * u;
}
/// @brief 1 - (1-u)^k（最初は速く、終わりに向かって減速）
inline float EaseOutPow(float u, float k) {
  u = std::clamp(u, 0.0f, 1.0f);
  return 1.0f - std::pow(1.0f - u, (std::max)(k, 0.1f));
}

/// @brief シーンのメインカメラ（CameraComponent::isMain）を探す
inline std::shared_ptr<Entity> FindMainCamera(Scene *scene) {
  if (!scene) return nullptr;
  for (const auto &e : scene->GetEntities()) {
    if (!e) continue;
    auto *cam = e->GetComponent<CameraComponent>();
    if (cam && cam->isMain && e->GetComponent<TransformComponent>()) return e;
  }
  return nullptr;
}

/// @brief シーンの水面の高さ（WaterComponent の Transform.y。無ければ 0）
inline float FindWaterY(Scene *scene) {
  if (!scene) return 0.0f;
  for (const auto &e : scene->GetEntities()) {
    if (!e || !e->GetComponent<WaterComponent>()) continue;
    if (auto *tr = e->GetComponent<TransformComponent>()) return tr->position.y;
    return 0.0f;
  }
  return 0.0f;
}

// 水面を出入りするときのレンズ演出（RailShooterController と同じ値）
inline constexpr float kRadialBlurDuration = 0.45f;
inline constexpr float kRadialBlurMaxWidth = 0.035f;
inline constexpr float kDropletDuration = 2.2f;
inline constexpr float kDropletDistortion = 0.06f;
inline constexpr float kDropletScale = 1.5f;

/// @brief ラジアルブラーとレンズ水滴の減衰を管理する
struct LensFx {
  float radialTimer = 0.0f;
  float radialMax = kRadialBlurMaxWidth;
  float dropletTimer = 0.0f;

  /// @param dropletSpeed 負で上へ昇る（着水）、正で下へ垂れる（出水）
  void Trigger(PostProcess *pp, float radialScale, float dropletSpeed) {
    if (!pp) return;
    radialMax = kRadialBlurMaxWidth * radialScale;
    radialTimer = kRadialBlurDuration * radialScale;
    pp->AddEffect(PostEffectType::RadialBlur);
    pp->SetRadialBlurCenter(0.5f, 0.5f);
    pp->SetRadialBlurWidth(radialMax);

    dropletTimer = kDropletDuration;
    pp->AddEffect(PostEffectType::ScreenDroplets);
    pp->SetScreenDropletsIntensity(1.0f);
    pp->SetScreenDropletsSpeed(dropletSpeed);
    pp->SetScreenDropletsDistortion(kDropletDistortion);
    pp->SetScreenDropletsScale(kDropletScale);
  }

  void Update(PostProcess *pp, float dt) {
    if (!pp) return;
    if (radialTimer > 0.0f) {
      radialTimer -= dt;
      if (radialTimer <= 0.0f) {
        radialTimer = 0.0f;
        pp->RemoveEffect(PostEffectType::RadialBlur);
      } else {
        const float p = radialTimer / (kRadialBlurDuration * (radialMax / kRadialBlurMaxWidth));
        pp->SetRadialBlurWidth(radialMax * p * p);
      }
    }
    if (dropletTimer > 0.0f) {
      dropletTimer -= dt;
      if (dropletTimer <= 0.0f) {
        dropletTimer = 0.0f;
        pp->RemoveEffect(PostEffectType::ScreenDroplets);
      } else {
        const float p = dropletTimer / kDropletDuration;
        pp->SetScreenDropletsIntensity(p * p);
      }
    }
  }

  bool Active() const { return radialTimer > 0.0f || dropletTimer > 0.0f; }

  void Clear(PostProcess *pp) {
    if (pp) {
      pp->RemoveEffect(PostEffectType::RadialBlur);
      pp->RemoveEffect(PostEffectType::ScreenDroplets);
    }
    radialTimer = dropletTimer = 0.0f;
  }
};

// ------------------------------------------------------------------
// 泡（GPU パーティクル）
// ------------------------------------------------------------------

/// @brief カメラの前方から昇る泡。GPU パーティクルは水面で消えないので、
///        射出点は常に水面から kMinDepth より下に抑え、寿命も昇りきらない長さにしてある。
class Bubbles {
public:
  static constexpr uint32_t kEmitPerFrame = 5;
  static constexpr float kMinDepth = 7.0f;  ///< 射出ボックス上端を水面からこの深さより下に保つ
  static constexpr float kBoxHalfH = 2.0f;

  /// @brief エミッタを作る（初期化で一瞬止まるので、演出の直前ではなく OnCreate で呼ぶ）
  void Spawn(Scene *scene, const std::string &name) {
    if (!scene || !entity_.expired()) return;
    auto e = scene->CreateEntity(name);
    auto &tr = e->AddComponent<TransformComponent>();
    tr.position = {0.0f, -200.0f, 0.0f}; // 使うまでは画面外
    auto &gpu = e->AddComponent<GPUParticleComponent>();
    if (auto *ps = gpu.particleSystem.get()) {
      ps->SetPipelinePrefix("gpu_particle_bubble");
      ps->SetParticleType(ParticleType::Default); // EmitParticle.CS：形状・速度・寿命・色を全部使う
      ps->SetBlendMode(kBlendModeNormal);
      ps->SetMaxParticles(1024);
      ps->SetEmitCount(0);
      ps->emitterShape_ = EmitterShape::Box;
      // 奥行きを薄くしてカメラの至近に湧かないようにする（至近の泡は画面いっぱいの玉になる）
      ps->shapeBoxSize_ = {12.0f, kBoxHalfH * 2.0f, 6.0f};
      // 速度は 1 フレームあたりの移動量（UpdateParticle.CS は translate += velocity）
      ps->baseVelocity_ = {0.0f, 0.045f, 0.0f}; // ≒ 2.7 m/s で上昇
      ps->velocityVariance_ = 0.015f;
      ps->gravity_ = 0.0f;
      // 最長 2.2s × 2.7m/s ≒ 6m < kMinDepth。水面から飛び出さない
      ps->minLifeTime_ = 1.2f;
      ps->maxLifeTime_ = 2.2f;
      ps->minScale_ = 0.06f;
      ps->maxScale_ = 0.2f;
      ps->startColor_ = {0.85f, 0.95f, 1.0f, 0.75f};
      ps->endColor_ = {0.85f, 0.95f, 1.0f, 0.0f};
    }
    scene->InitDynamicEntityRuntime(*e);
    entity_ = e;
  }

  void SetEmission(uint32_t perFrame) {
    auto e = entity_.lock();
    if (!e) return;
    if (auto *gpu = e->GetComponent<GPUParticleComponent>()) {
      if (gpu->particleSystem) gpu->particleSystem->SetEmitCount(perFrame);
    }
  }

  /// @brief カメラの見ている方向の少し先・少し下へエミッタを置く
  void Follow(const TransformComponent &cam, float waterY) {
    auto e = entity_.lock();
    if (!e) return;
    auto *tr = e->GetComponent<TransformComponent>();
    if (!tr) return;
    // rotation.x = +π/2 で真下、0 で +Z、負で見上げ
    const float rx = cam.rotation.x;
    const RC::Vector3 fwd = {0.0f, -std::sin(rx), std::cos(rx)};
    float y = cam.position.y + fwd.y * 8.0f - 3.0f;
    y = (std::min)(y, waterY - kMinDepth - kBoxHalfH);
    tr->position = {cam.position.x + fwd.x * 8.0f, y, cam.position.z + fwd.z * 8.0f};
  }

  void Destroy() {
    if (auto e = entity_.lock()) e->Destroy();
    entity_.reset();
  }

private:
  std::weak_ptr<Entity> entity_;
};

// ------------------------------------------------------------------
// 飛び込み
// ------------------------------------------------------------------

/// @brief 飛び込みの調整値（JSON キーは "dive*"。Title / Result 共通）
struct DiveParams {
  bool enabled = true;
  float anticipation = 0.35f; ///< ため（秒）
  float lift = 1.5f;          ///< ための持ち上がり（m）
  float plunge = 0.85f;       ///< 水面までの落下（秒）
  float fovBoost = 1.35f;     ///< 落下中の画角の広がり（倍率）
  float sink = 1.7f;          ///< 着水から depth まで沈む時間（秒）
  float depth = 30.0f;        ///< 沈む深さ（m）。UnderwaterLook の deepDepth より深くしておく
  float lookUpPitch = -0.45f; ///< 沈みながら向く角度（rad。負で見上げ）
  float tilt = 0.9f;          ///< 真下向き → lookUpPitch にかける時間（秒）
  float surfaceBlend = 0.25f; ///< 着水後、水中エフェクトが掛かりきるまで（秒）
  bool bubbles = true;

  void WriteJson(nlohmann::json &j) const {
    j["diveEnabled"] = enabled;
    j["diveAnticipation"] = anticipation;
    j["diveLift"] = lift;
    j["divePlunge"] = plunge;
    j["diveFovBoost"] = fovBoost;
    j["diveSink"] = sink;
    j["diveDepth"] = depth;
    j["diveLookUpPitch"] = lookUpPitch;
    j["diveTilt"] = tilt;
    j["diveSurfaceBlend"] = surfaceBlend;
    j["diveBubbles"] = bubbles;
  }
  void ReadJson(const nlohmann::json &j) {
    auto f = [&](const char *k, float &o) { if (j.contains(k) && j[k].is_number()) o = j[k].get<float>(); };
    auto b = [&](const char *k, bool &o) { if (j.contains(k) && j[k].is_boolean()) o = j[k].get<bool>(); };
    b("diveEnabled", enabled);
    f("diveAnticipation", anticipation);
    f("diveLift", lift);
    f("divePlunge", plunge);
    f("diveFovBoost", fovBoost);
    f("diveSink", sink);
    f("diveDepth", depth);
    f("diveLookUpPitch", lookUpPitch);
    f("diveTilt", tilt);
    f("diveSurfaceBlend", surfaceBlend);
    b("diveBubbles", bubbles);
  }
#if RC_ENABLE_IMGUI
  void DrawImGui() {
    ImGui::Checkbox("Dive Enabled", &enabled);
    ImGui::DragFloat("Dive Anticipation (s)", &anticipation, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Dive Lift (m)", &lift, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("Dive Plunge (s)", &plunge, 0.01f, 0.1f, 5.0f);
    ImGui::DragFloat("Dive FOV Boost", &fovBoost, 0.01f, 1.0f, 2.0f);
    ImGui::DragFloat("Dive Sink (s)", &sink, 0.01f, 0.1f, 10.0f);
    ImGui::DragFloat("Dive Depth (m)", &depth, 0.5f, 1.0f, 200.0f);
    ImGui::DragFloat("Dive Look Up Pitch", &lookUpPitch, 0.01f, -1.5f, 1.5708f);
    ImGui::DragFloat("Dive Tilt (s)", &tilt, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Dive Surface Blend (s)", &surfaceBlend, 0.01f, 0.0f, 2.0f);
    ImGui::Checkbox("Dive Bubbles", &bubbles);
  }
#endif
};

/// @brief 真上視点のカメラを水へ飛び込ませ、深海色まで沈める
/// @details ため → 落下（画角が広がる）→ 着水（水中一式＋ラジアルブラー＋上へ昇る水滴＋泡）
///          → 見上げながら沈む → 深さが look.deepDepth に達したら IsDone()。
///          IsDone() のあいだも緩く沈み続けるので、呼び出し側はその間に遷移を要求する。
class DiveSequence {
public:
  enum class Phase { None, Anticipate, Plunge, Sink, Done };

  bool Begin(std::shared_ptr<Entity> cam, float waterY, const DiveParams &p,
             const UnderwaterLook::Params &look, Bubbles *bubbles) {
    if (phase_ != Phase::None) return true;
    if (!cam) return false;
    auto *tr = cam->GetComponent<TransformComponent>();
    auto *cc = cam->GetComponent<CameraComponent>();
    if (!tr || !cc) return false;

    *this = DiveSequence{};
    p_ = &p;
    look_ = &look;
    bubbles_ = bubbles;
    camera_ = cam;
    basePos_ = tr->position;
    baseRot_ = tr->rotation;
    baseFov_ = cc->fovY;
    waterY_ = waterY;
    depth_ = waterY - tr->position.y;
    phase_ = (p.anticipation > 0.0f) ? Phase::Anticipate : Phase::Plunge;
    return true;
  }

  /// @param restoreCamera カメラの位置・向き・画角を開始時へ戻すか（中断用）
  void End(bool restoreCamera) {
    PostProcess *pp = RC::GetRenderContext().GetPostProcess();
    if (effectsOn_) UnderwaterLook::RemoveStack(pp);
    lens_.Clear(pp);
    if (bubbles_) bubbles_->SetEmission(0);
    if (restoreCamera) {
      if (auto cam = camera_.lock()) {
        if (auto *tr = cam->GetComponent<TransformComponent>()) {
          tr->position = basePos_;
          tr->rotation = baseRot_;
        }
        if (auto *cc = cam->GetComponent<CameraComponent>()) cc->fovY = baseFov_;
      }
    }
    *this = DiveSequence{};
  }

  /// @return カメラが消えていたら false（呼び出し側は演出を諦める）
  bool Update(float dt) {
    if (phase_ == Phase::None || !p_ || !look_) return true;
    auto cam = camera_.lock();
    if (!cam) return false;
    auto *tr = cam->GetComponent<TransformComponent>();
    auto *cc = cam->GetComponent<CameraComponent>();
    if (!tr || !cc) return false;
    const DiveParams &p = *p_;

    t_ += dt;
    switch (phase_) {
    case Phase::Anticipate: {
      const float u = t_ / (std::max)(p.anticipation, 1e-3f);
      tr->position.y = basePos_.y + p.lift * EaseOutCubic(u);
      if (u >= 1.0f) { phase_ = Phase::Plunge; t_ = 0.0f; }
      break;
    }
    case Phase::Plunge: {
      const float u = t_ / (std::max)(p.plunge, 1e-3f);
      const float e = EaseInQuad(u);
      const float startY = basePos_.y + ((p.anticipation > 0.0f) ? p.lift : 0.0f);
      tr->position.y = startY + (waterY_ - startY) * e;
      cc->fovY = baseFov_ * (1.0f + (p.fovBoost - 1.0f) * e);
      if (u >= 1.0f) {
        tr->position.y = waterY_;
        phase_ = Phase::Sink;
        t_ = 0.0f;
        OnSplash();
      }
      break;
    }
    case Phase::Sink: {
      const float u = t_ / (std::max)(p.sink, 1e-3f);
      tr->position.y = waterY_ - p.depth * EaseOutQuad(u);
      const float tilt = (p.tilt > 0.0f) ? Smooth01(t_ / p.tilt) : 1.0f;
      tr->rotation.x = baseRot_.x + (p.lookUpPitch - baseRot_.x) * tilt;
      cc->fovY = baseFov_ * (p.fovBoost - (p.fovBoost - 1.0f) * EaseOutQuad(u));
      // 画面が深海色になりきった時点で終わり（沈みきるまで待つと暗い時間が延びるだけ）
      if (u >= 1.0f || (waterY_ - tr->position.y) >= look_->deepDepth) {
        phase_ = Phase::Done;
        t_ = 0.0f;
      }
      break;
    }
    case Phase::Done:
      tr->position.y -= kDoneSinkSpeed * dt; // 遷移が始まるまで止まって見えないように
      break;
    case Phase::None:
      break;
    }

    depth_ = waterY_ - tr->position.y;

    if (effectsOn_) {
      submerged_ += dt;
      PostProcess *pp = RC::GetRenderContext().GetPostProcess();
      const float blend = (p.surfaceBlend > 0.0f) ? Smooth01(submerged_ / p.surfaceBlend) : 1.0f;
      UnderwaterLook::SetLerp(pp, blend);
      UnderwaterLook::ApplyFogByDepth(pp, *look_, depth_);
      // 暗くなりきる前に水滴を消しきる（残ったまま Game へ切り替わると一瞬で消えて見える）
      if (phase_ == Phase::Done) lens_.dropletTimer = (std::min)(lens_.dropletTimer, 0.2f);
      lens_.Update(pp, dt);
    }
    if (bubbles_) bubbles_->Follow(*tr, waterY_);
    return true;
  }

  bool IsActive() const { return phase_ != Phase::None; }
  bool IsDone() const { return phase_ == Phase::Done; }
  float DoneTime() const { return phase_ == Phase::Done ? t_ : 0.0f; }
  Phase GetPhase() const { return phase_; }
  float Depth() const { return depth_; }

  static const char *PhaseName(Phase ph) {
    switch (ph) {
    case Phase::None: return "none";
    case Phase::Anticipate: return "anticipate";
    case Phase::Plunge: return "plunge";
    case Phase::Sink: return "sink";
    case Phase::Done: return "done";
    }
    return "?";
  }

private:
  static constexpr float kDoneSinkSpeed = 4.0f;

  void OnSplash() {
    submerged_ = 0.0f;
    if (PostProcess *pp = RC::GetRenderContext().GetPostProcess()) {
      UnderwaterLook::SetupLight(pp, *look_, waterY_);
      UnderwaterLook::AddStack(pp);
      UnderwaterLook::SetLerp(pp, 0.0f);
      UnderwaterLook::ApplyFogByDepth(pp, *look_, 0.0f);
      lens_.Trigger(pp, 1.0f, -1.3f); // 着水：泡が上へ昇る
      effectsOn_ = true;
    }
    if (bubbles_ && p_->bubbles) bubbles_->SetEmission(Bubbles::kEmitPerFrame);
    Log::Print("[WaterCameraFx] dive: splash");
  }

  const DiveParams *p_ = nullptr;
  const UnderwaterLook::Params *look_ = nullptr;
  Bubbles *bubbles_ = nullptr;
  std::weak_ptr<Entity> camera_;
  Phase phase_ = Phase::None;
  float t_ = 0.0f;
  RC::Vector3 basePos_{};
  RC::Vector3 baseRot_{};
  float baseFov_ = 0.45f;
  float waterY_ = 0.0f;
  float depth_ = 0.0f;
  float submerged_ = 0.0f;
  bool effectsOn_ = false;
  LensFx lens_;
};

// ------------------------------------------------------------------
// 浮上（深海 → 水面 → 真上視点へ上昇）
// ------------------------------------------------------------------

/// @brief 浮上の調整値（JSON キーは "rise*"）
struct RiseParams {
  bool enabled = true;
  float depth = 26.0f;        ///< 浮上を始める深さ（m）。UnderwaterLook の deepDepth より深く
  float underwaterTime = 2.4f;///< 水面の直下まで上がる時間（秒）
  float ease = 2.2f;          ///< 水中の速度カーブ（1 - (1-t)^ease。暗い深海は速く抜ける）
  float pitchStart = -0.35f;  ///< 浮上開始時に見上げる角度（rad）
  float climbTime = 2.0f;     ///< 水面を抜けてから最終位置（真上視点）までの時間（秒）
  float surfaceBlend = 0.5f;  ///< 水面を抜けてから水中エフェクトが消えきるまで（秒）
  bool bubbles = true;

  void WriteJson(nlohmann::json &j) const {
    j["riseEnabled"] = enabled;
    j["riseDepth"] = depth;
    j["riseUnderwaterTime"] = underwaterTime;
    j["riseEase"] = ease;
    j["risePitchStart"] = pitchStart;
    j["riseClimbTime"] = climbTime;
    j["riseSurfaceBlend"] = surfaceBlend;
    j["riseBubbles"] = bubbles;
  }
  void ReadJson(const nlohmann::json &j) {
    auto f = [&](const char *k, float &o) { if (j.contains(k) && j[k].is_number()) o = j[k].get<float>(); };
    auto b = [&](const char *k, bool &o) { if (j.contains(k) && j[k].is_boolean()) o = j[k].get<bool>(); };
    b("riseEnabled", enabled);
    f("riseDepth", depth);
    f("riseUnderwaterTime", underwaterTime);
    f("riseEase", ease);
    f("risePitchStart", pitchStart);
    f("riseClimbTime", climbTime);
    f("riseSurfaceBlend", surfaceBlend);
    b("riseBubbles", bubbles);
  }
#if RC_ENABLE_IMGUI
  void DrawImGui() {
    ImGui::Checkbox("Rise Enabled", &enabled);
    ImGui::DragFloat("Rise Depth (m)", &depth, 0.5f, 1.0f, 200.0f);
    ImGui::DragFloat("Rise Underwater (s)", &underwaterTime, 0.05f, 0.2f, 10.0f);
    ImGui::DragFloat("Rise Ease", &ease, 0.05f, 1.0f, 5.0f);
    ImGui::DragFloat("Rise Pitch Start", &pitchStart, 0.01f, -1.5f, 1.5f);
    ImGui::DragFloat("Rise Climb (s)", &climbTime, 0.05f, 0.2f, 10.0f);
    ImGui::DragFloat("Rise Surface Blend (s)", &surfaceBlend, 0.01f, 0.0f, 3.0f);
    ImGui::Checkbox("Rise Bubbles", &bubbles);
  }
#endif
};

/// @brief カメラを深海に置き、水面へ浮上させてから元の位置・向き（真上視点など）へ上昇させる
/// @details Begin の時点でカメラが置かれている位置・向きが「最終位置」になる。
///          1. 水中  : 深海から水面の直下まで。暗い深海は速く抜け、水面の手前で減速する。
///                     向きは見上げ → 水平へ。深さに応じてフォグが明るくなる。
///          2. 上昇  : 水面を抜け（ラジアルブラー＋下へ垂れる水滴）、水平線を見ながら上がり、
///                     最後に最終の向き（真上視点なら真下）へ傾く。
///          水中エフェクトは Begin で積み、水面を抜けたら surfaceBlend 秒で外す。
class RiseSequence {
public:
  enum class Phase { None, Underwater, Climb, Done };

  bool Begin(std::shared_ptr<Entity> cam, float waterY, const RiseParams &p,
             const UnderwaterLook::Params &look, Bubbles *bubbles) {
    if (!cam) return false;
    auto *tr = cam->GetComponent<TransformComponent>();
    if (!tr) return false;

    *this = RiseSequence{};
    p_ = &p;
    look_ = &look;
    bubbles_ = bubbles;
    camera_ = cam;
    waterY_ = waterY;
    goalPos_ = tr->position;
    goalRot_ = tr->rotation;
    startPos_ = {goalPos_.x, waterY - p.depth, goalPos_.z};
    startRot_ = {p.pitchStart, goalRot_.y, goalRot_.z};
    surfacePos_ = {goalPos_.x, waterY - kUnderSurface, goalPos_.z};
    tr->position = startPos_;
    tr->rotation = startRot_;

    if (PostProcess *pp = RC::GetRenderContext().GetPostProcess()) {
      UnderwaterLook::SetupLight(pp, look, waterY);
      UnderwaterLook::AddStack(pp);
      UnderwaterLook::SetLerp(pp, 1.0f);
      UnderwaterLook::ApplyFogByDepth(pp, look, p.depth);
      effectsOn_ = true;
    }
    if (bubbles_ && p.bubbles && p.depth > Bubbles::kMinDepth + 1.0f) {
      bubbles_->SetEmission(Bubbles::kEmitPerFrame);
      emitting_ = true;
    }
    phase_ = Phase::Underwater;
    Log::Print("[WaterCameraFx] rise begin");
    return true;
  }

  /// @brief 途中で止める。カメラは最終位置へ置き、画面効果は外す
  void End() {
    if (auto cam = camera_.lock()) {
      if (auto *tr = cam->GetComponent<TransformComponent>()) {
        tr->position = goalPos_;
        tr->rotation = goalRot_;
      }
    }
    PostProcess *pp = RC::GetRenderContext().GetPostProcess();
    if (effectsOn_) UnderwaterLook::RemoveStack(pp);
    lens_.Clear(pp);
    if (bubbles_) bubbles_->SetEmission(0);
    *this = RiseSequence{};
  }

  void Update(float dt) {
    if (phase_ == Phase::None || !p_ || !look_) return;
    PostProcess *pp = RC::GetRenderContext().GetPostProcess();
    auto cam = camera_.lock();
    auto *tr = cam ? cam->GetComponent<TransformComponent>() : nullptr;
    if (!tr) { End(); return; }
    const RiseParams &p = *p_;

    t_ += dt;
    if (phase_ == Phase::Underwater) {
      const float u = t_ / (std::max)(p.underwaterTime, 1e-3f);
      const float e = EaseOutPow(u, p.ease);
      tr->position = RC::Lerp(startPos_, surfacePos_, e);
      tr->rotation = RC::Lerp(startRot_, {0.0f, goalRot_.y, goalRot_.z}, Smooth01(u));
      if (u >= 1.0f) { phase_ = Phase::Climb; t_ = 0.0f; }
    } else if (phase_ == Phase::Climb) {
      const float u = t_ / (std::max)(p.climbTime, 1e-3f);
      tr->position = RC::Lerp(surfacePos_, goalPos_, Smooth01(u));
      // 水面を抜けた直後は水平線を見せ、上がりきる手前で最終の向きへ傾く
      const float r = Smooth01((u - 0.2f) / 0.8f);
      tr->rotation = RC::Lerp({0.0f, goalRot_.y, goalRot_.z}, goalRot_, r);
      if (u >= 1.0f) {
        tr->position = goalPos_;
        tr->rotation = goalRot_;
        phase_ = Phase::Done;
        t_ = 0.0f;
        Log::Print("[WaterCameraFx] rise finished");
      }
    }

    const float depth = waterY_ - tr->position.y;

    // 泡は水面に届かない深さまでで止める
    if (emitting_ && depth < Bubbles::kMinDepth + 1.0f) {
      bubbles_->SetEmission(0);
      emitting_ = false;
    }
    if (bubbles_) bubbles_->Follow(*tr, waterY_);

    // 水面を抜けた瞬間：出水の演出を出し、水中エフェクトを外し始める
    if (effectsOn_ && !surfaced_ && depth <= 0.0f) {
      surfaced_ = true;
      surfacedTime_ = 0.0f;
      lens_.Trigger(pp, 0.7f, 1.3f); // 出水：水滴が下へ垂れる
    }
    if (effectsOn_) {
      if (!surfaced_) {
        UnderwaterLook::ApplyFogByDepth(pp, *look_, depth);
      } else {
        surfacedTime_ += dt;
        const float blend = (p.surfaceBlend > 0.0f) ? 1.0f - Smooth01(surfacedTime_ / p.surfaceBlend) : 0.0f;
        UnderwaterLook::SetLerp(pp, blend);
        if (blend <= 0.0f) {
          UnderwaterLook::RemoveStack(pp);
          effectsOn_ = false;
        }
      }
    }
    lens_.Update(pp, dt);

    if (phase_ == Phase::Done && !effectsOn_ && !lens_.Active()) {
      if (bubbles_) bubbles_->SetEmission(0);
      phase_ = Phase::None;
    }
  }

  /// @brief カメラがまだ動いているか（入力を止める判定に使う）
  bool IsMoving() const { return phase_ == Phase::Underwater || phase_ == Phase::Climb; }
  /// @brief 画面効果の後始末まで含めてまだ動いているか
  bool IsActive() const { return phase_ != Phase::None; }

private:
  static constexpr float kUnderSurface = 0.6f; ///< 水中フェーズの終点（水面の少し下）

  const RiseParams *p_ = nullptr;
  const UnderwaterLook::Params *look_ = nullptr;
  Bubbles *bubbles_ = nullptr;
  std::weak_ptr<Entity> camera_;
  Phase phase_ = Phase::None;
  float t_ = 0.0f;
  float waterY_ = 0.0f;
  RC::Vector3 startPos_{}, surfacePos_{}, goalPos_{};
  RC::Vector3 startRot_{}, goalRot_{};
  bool effectsOn_ = false;
  bool surfaced_ = false;
  float surfacedTime_ = 0.0f;
  bool emitting_ = false;
  LensFx lens_;
};

} // namespace WaterCameraFx
