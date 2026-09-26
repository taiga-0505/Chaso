#include "EnemyBaseScript.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/CameraComponent.h"
#include "ECS/ColliderComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/WaterComponent.h"
#include "Common/Log/Log.h"
#include "Scene.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

// ============================================================================
// タコ（墨を吐く敵）
//
//   - 自機の前方 preferredDistance の位置に、ジェット推進（ドン…スーッ）で回り込む
//   - 射程内で前方にいると、体を膨らませて「ため」→ 墨玉（InkBlobScript）を吐く
//   - 墨玉が自機に届くと画面に墨が貼り付き、しばらく視界が悪くなる（InkScreenFx）
//   - 墨玉は撃ち落とせる。ため中にタコを倒せば吐かれない
//
//   サメ（突っ込んで HP を削る）・船（遠くから砲撃で HP を削る）に対して、
//   タコは HP ではなく「視界」を削る。ダメージは既定 0（inkDamage で変更可）。
//
//   スクリプト間の通信は他の敵と同じくタグのみ：
//     墨玉 → 自機  : pending_ink（強さ×100 の整数を加算）/ pending_damage
// ============================================================================

namespace OctopusDetail {

constexpr float kPi = 3.14159265f;

/// @brief シーンのメインカメラ（isMain 優先。無ければ最初のカメラ）
inline std::shared_ptr<Entity> FindPlayerCamera(Scene *scene) {
  if (!scene) return nullptr;
  std::shared_ptr<Entity> fallback;
  for (const auto &e : scene->GetEntities()) {
    if (!e || !e->IsActive() || e->IsPendingDestroy()) continue;
    auto *cam = e->GetComponent<CameraComponent>();
    if (!cam || !e->GetComponent<TransformComponent>()) continue;
    if (cam->isMain) return e;
    if (!fallback) fallback = e;
  }
  return fallback;
}

/// @brief シーンの水面の高さ（WaterComponent の Transform.y）。無ければ false
inline bool FindWaterY(Scene *scene, float &outY) {
  if (!scene) return false;
  for (const auto &e : scene->GetEntities()) {
    if (!e || !e->GetComponent<WaterComponent>()) continue;
    auto *tr = e->GetComponent<TransformComponent>();
    outY = tr ? tr->position.y : 0.0f;
    return true;
  }
  return false;
}

inline float Len(const RC::Vector3 &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

} // namespace OctopusDetail

// ============================================================================
// 墨玉がはじける演出
// ============================================================================

/// @brief 墨のはじけ（飛沫・閃光・墨の煙）1 粒ぶんの動き
/// @details パラメータは生成側がタグで渡す（プールから使い回すため OnCreate では受け取れない）。
///          `ink_kind` 0=飛沫 / 1=墨の煙 / 2=閃光。速度・大きさ・寿命・不透明度は 1000 倍の整数。
///          Splash と同じく、終わったら非アクティブにしてプールへ戻す。
class InkParticleScript : public ScriptableEntity {
protected:
  void OnUpdate(float dt) override {
    Entity *self = GetEntity();
    if (!self) return;
    if (self->GetTagInt("reused", 0) == 1 || !started_) {
      self->ClearTag("reused");
      Start(*self);
    }
    if (finished_ || dt <= 0.0f) return;

    auto *tr = GetComponent<TransformComponent>();
    if (!tr) return;

    elapsed_ += dt;
    const float t = elapsed_ / (std::max)(life_, 0.01f);
    if (t >= 1.0f) {
      finished_ = true;
      self->SetActive(false);
      return;
    }

    // 水の抵抗（陰的）＋ゆるい重力
    const float k = 1.0f / (1.0f + drag_ * dt);
    vel_.x *= k; vel_.y *= k; vel_.z *= k;
    vel_.y -= gravity_ * dt;
    tr->position.x += vel_.x * dt;
    tr->position.y += vel_.y * dt;
    tr->position.z += vel_.z * dt;

    float scale = s0_;
    float alpha = a0_;
    const float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
    switch (kind_) {
    case 0: // 飛沫：縮みながら、最後の 4 割で消える
      scale = s0_ * (1.0f - t * t);
      alpha = a0_ * (1.0f - std::clamp((t - 0.6f) / 0.4f, 0.0f, 1.0f));
      break;
    case 1: // 墨の煙：ふわっと広がって薄れる
      scale = s0_ + (s1_ - s0_) * easeOut;
      alpha = a0_ * std::pow(1.0f - t, 1.5f);
      break;
    default: // 閃光：一瞬で膨らんで消える
      scale = s0_ + (s1_ - s0_) * easeOut;
      alpha = a0_ * (1.0f - t) * (1.0f - t);
      break;
    }
    scale = (std::max)(scale, 0.005f);
    tr->scale = {scale, scale, scale};

    if (auto *pm = GetComponent<PrimitiveMeshComponent>()) {
      if (pm->meshHandle >= 0) {
        if (auto *mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) mat->color.w = alpha;
      }
    }
  }

private:
  void Start(Entity &self) {
    started_ = true;
    finished_ = false;
    elapsed_ = 0.0f;
    auto f = [&](const char *key, float def) {
      return static_cast<float>(self.GetTagInt(key, static_cast<int>(def * 1000.0f))) / 1000.0f;
    };
    kind_ = self.GetTagInt("ink_kind", 0);
    vel_ = {f("ink_vx", 0.0f), f("ink_vy", 0.0f), f("ink_vz", 0.0f)};
    s0_ = f("ink_s0", 0.2f);
    s1_ = f("ink_s1", 0.2f);
    life_ = f("ink_life", 0.6f);
    a0_ = f("ink_a0", 1.0f);
    drag_ = f("ink_drag", 2.5f);
    gravity_ = f("ink_grav", 5.0f);
  }

  int kind_ = 0;
  RC::Vector3 vel_ = {0.0f, 0.0f, 0.0f};
  float s0_ = 0.2f, s1_ = 0.2f, life_ = 0.6f, a0_ = 1.0f, drag_ = 2.5f, gravity_ = 5.0f;
  float elapsed_ = 0.0f;
  bool started_ = false;
  bool finished_ = false;
};

REGISTER_SCRIPT(InkParticleScript)

namespace OctopusDetail {

/// @brief 墨玉がはじける演出を出す
/// @param pos     はじけた位置
/// @param dir     墨玉が飛んでいた向き（飛沫が少しだけ前へ流れる）
/// @param size    墨玉の大きさ（Transform の scale）
inline void SpawnInkBurst(Scene *scene, SceneContext *ctx, const RC::Vector3 &pos,
                          const RC::Vector3 &dir, float size) {
  if (!scene) return;

  static std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> U(0.0f, 1.0f);

  struct Spec {
    int kind;
    RC::Vector3 vel;
    float s0, s1, life, drag, grav;
    RC::Vector4 color;
  };
  std::vector<Spec> specs;

  // 閃光：紫がかった光が一瞬ふくらむ（「パンッ」の瞬間）
  specs.push_back({2, {0, 0, 0}, size * 0.6f, size * 2.6f, 0.16f, 0.0f, 0.0f, {0.80f, 0.60f, 1.0f, 0.75f}});

  // 墨の煙：その場に残ってふわっと広がる
  for (int i = 0; i < 3; ++i) {
    const RC::Vector3 v = {(U(rng) - 0.5f) * 2.0f, (U(rng) - 0.3f) * 1.2f, (U(rng) - 0.5f) * 2.0f};
    specs.push_back({1, v, size * 0.7f, size * (2.0f + U(rng) * 0.8f), 0.8f + U(rng) * 0.4f, 1.5f, -0.3f,
                     {0.09f, 0.05f, 0.13f, 0.55f}});
  }

  // 飛沫：全方向へ飛び散る。墨玉の勢いで少し前へ流れる
  for (int i = 0; i < 14; ++i) {
    // 球面上に一様に散らす
    const float z = U(rng) * 2.0f - 1.0f;
    const float a = U(rng) * 6.2831853f;
    const float r = std::sqrt((std::max)(0.0f, 1.0f - z * z));
    const float sp = 6.0f + U(rng) * 7.0f;
    const RC::Vector3 v = {std::cos(a) * r * sp + dir.x * 3.0f,
                           z * sp + dir.y * 3.0f + 1.5f,
                           std::sin(a) * r * sp + dir.z * 3.0f};
    const float s = size * (0.16f + U(rng) * 0.22f);
    specs.push_back({0, v, s, s, 0.45f + U(rng) * 0.35f, 2.5f, 6.0f, {0.06f, 0.03f, 0.09f, 1.0f}});
  }

  // エフェクトフォルダ（WaterBullet と同じ場所へまとめる）と、使い回せる粒を集める
  uint64_t folder = 0;
  std::vector<std::shared_ptr<Entity>> pool;
  for (const auto &e : scene->GetEntities()) {
    if (!e) continue;
    if (folder == 0 && e->IsFolder() && e->GetName() == "Effects") folder = e->Guid();
    if (!e->IsActive() && !e->IsPendingDestroy() && e->GetName() == "InkParticle" && pool.size() < specs.size()) {
      pool.push_back(e);
    }
  }
  if (folder == 0) {
    auto f = scene->CreateEntity("Effects");
    f->SetIsFolder(true);
    folder = f->Guid();
  }

  auto toTag = [](float v) { return static_cast<int>(v * 1000.0f); };
  for (size_t i = 0; i < specs.size(); ++i) {
    const Spec &sp = specs[i];
    std::shared_ptr<Entity> e;
    const bool isNew = (i >= pool.size());
    if (!isNew) {
      e = pool[i];
      e->SetActive(true);
      e->SetTag("reused", 1);
    } else {
      e = scene->CreateEntity("InkParticle");
      if (!e) continue;
    }
    e->SetParentGuid(folder);

    auto *tr = e->GetComponent<TransformComponent>();
    if (!tr) tr = &e->AddComponent<TransformComponent>();
    tr->position = pos;
    tr->scale = {sp.s0, sp.s0, sp.s0};

    auto *pm = e->GetComponent<PrimitiveMeshComponent>();
    if (!pm) {
      pm = &e->AddComponent<PrimitiveMeshComponent>();
      pm->type = PrimitiveType::Sphere;
    }
    pm->color = sp.color;

    e->SetTag("ink_kind", sp.kind);
    e->SetTag("ink_vx", toTag(sp.vel.x));
    e->SetTag("ink_vy", toTag(sp.vel.y));
    e->SetTag("ink_vz", toTag(sp.vel.z));
    e->SetTag("ink_s0", toTag(sp.s0));
    e->SetTag("ink_s1", toTag(sp.s1));
    e->SetTag("ink_life", toTag(sp.life));
    e->SetTag("ink_a0", toTag(sp.color.w));
    e->SetTag("ink_drag", toTag(sp.drag));
    e->SetTag("ink_grav", toTag(sp.grav));

    auto *nsc = e->GetComponent<NativeScriptComponent>();
    if (!nsc) {
      nsc = &e->AddComponent<NativeScriptComponent>();
      nsc->AddScript("InkParticleScript");
      nsc->SetScene(scene);
      if (ctx) nsc->SetSceneContext(ctx);
    }

    if (isNew) scene->InitDynamicEntityRuntime(*e);

    // 色（使い回しの粒は前回の色と透明度が残っている）と Transform をその場で反映する
    if (pm->meshHandle >= 0) {
      if (auto *mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) mat->color = sp.color;
      if (auto *pmTr = RC::GetPrimitiveMeshTransformPtr(pm->meshHandle)) {
        pmTr->scale = tr->scale;
        pmTr->rotation = tr->rotation;
        pmTr->translation = tr->position;
      }
    }
  }
}

} // namespace OctopusDetail

// ============================================================================
// 墨玉
// ============================================================================

/// @brief タコが吐く墨玉。自機へゆるく追尾して飛び、届いたら画面に墨を貼る
/// @details WaterBullet は水面（y=0）を跨ぐと消えるうえ、当たり判定が is_player タグ依存なので、
///          水中から水上の自機へも届くよう専用に持つ。
///          撃ち落とし判定も自前で行う（is_enemy を付けると敵 HP の集計や HP バーに混ざるため）。
class InkBlobScript : public ScriptableEntity {
public:
  float speed = 13.0f;        ///< 飛ぶ速さ (m/s)
  float homingRate = 1.1f;    ///< 追尾の強さ（rad/s）。0 で直進
  float lifetime = 5.0f;      ///< 何秒で消えるか
  float hitRadius = 1.8f;     ///< 自機（カメラ）にこの距離まで来たら命中
  float shootRadius = 1.3f;   ///< 自機の弾がこの距離まで来たら撃ち落とされる
  float inkAmount = 1.0f;     ///< 命中時に貼る墨の強さ 0..1
  int damage = 0;             ///< 命中時のダメージ（既定は視界だけ奪う）
  RC::Vector3 direction = {0.0f, 0.0f, 1.0f};

protected:
  nlohmann::json Serialize() override {
    return {
        {"speed", speed},
        {"homingRate", homingRate},
        {"lifetime", lifetime},
        {"hitRadius", hitRadius},
        {"shootRadius", shootRadius},
        {"inkAmount", inkAmount},
        {"damage", damage},
        {"dir", {direction.x, direction.y, direction.z}},
    };
  }

  void Deserialize(const nlohmann::json &j) override {
    auto f = [&](const char *k, float &o) { if (j.contains(k) && j[k].is_number()) o = j[k].get<float>(); };
    f("speed", speed);
    f("homingRate", homingRate);
    f("lifetime", lifetime);
    f("hitRadius", hitRadius);
    f("shootRadius", shootRadius);
    f("inkAmount", inkAmount);
    if (j.contains("damage") && j["damage"].is_number()) damage = j["damage"].get<int>();
    if (j.contains("dir") && j["dir"].is_array() && j["dir"].size() == 3) {
      direction = {j["dir"][0].get<float>(), j["dir"][1].get<float>(), j["dir"][2].get<float>()};
    }
  }

  void OnCreate() override {
    const float l = OctopusDetail::Len(direction);
    if (l > 1e-4f) direction = {direction.x / l, direction.y / l, direction.z / l};
    else direction = {0.0f, 0.0f, 1.0f};
    elapsed_ = 0.0f;
    if (auto *tr = GetComponent<TransformComponent>()) baseScale_ = tr->scale;
  }

  void OnUpdate(float dt) override {
    if (done_ || dt <= 0.0f) return;
    auto *tr = GetComponent<TransformComponent>();
    Scene *scene = GetScene();
    if (!tr || !scene) return;

    elapsed_ += dt;
    if (elapsed_ >= lifetime) { Finish(); return; }

    std::shared_ptr<Entity> cam = target_.lock();
    if (!cam || cam->IsPendingDestroy() || !cam->IsActive()) {
      cam = OctopusDetail::FindPlayerCamera(scene);
      target_ = cam;
    }

    // --- 追尾：向きを目標方向へ最大 homingRate [rad/s] だけ回す ---
    if (cam) {
      if (auto *camTr = cam->GetComponent<TransformComponent>()) {
        RC::Vector3 to = {camTr->position.x - tr->position.x,
                          camTr->position.y - tr->position.y,
                          camTr->position.z - tr->position.z};
        const float dist = OctopusDetail::Len(to);

        // 命中
        if (dist <= hitRadius) {
          const int ink = cam->GetTagInt("pending_ink", 0);
          cam->SetTag("pending_ink", ink + static_cast<int>(std::clamp(inkAmount, 0.0f, 1.0f) * 100.0f));
          if (damage > 0) {
            cam->SetTag("pending_damage", cam->GetTagInt("pending_damage", 0) + damage);
          }
          Log::Print("[InkBlobScript] Ink hit the player!");
          Finish();
          return;
        }

        if (dist > 1e-3f && homingRate > 0.0f) {
          to = {to.x / dist, to.y / dist, to.z / dist};
          const float d = std::clamp(direction.x * to.x + direction.y * to.y + direction.z * to.z, -1.0f, 1.0f);
          const float angle = std::acos(d);
          const float maxStep = homingRate * dt;
          if (angle > 1e-4f) {
            const float t = (std::min)(1.0f, maxStep / angle);
            RC::Vector3 nd = {direction.x + (to.x - direction.x) * t,
                              direction.y + (to.y - direction.y) * t,
                              direction.z + (to.z - direction.z) * t};
            const float l = OctopusDetail::Len(nd);
            if (l > 1e-4f) direction = {nd.x / l, nd.y / l, nd.z / l};
          }
        }
      }
    }

    tr->position.x += direction.x * speed * dt;
    tr->position.y += direction.y * speed * dt;
    tr->position.z += direction.z * speed * dt;

    // ぷるぷる脈打たせる（ただの黒い球に見せない）
    const float wob = std::sin(elapsed_ * 18.0f) * 0.12f;
    tr->scale = {baseScale_.x * (1.0f + wob), baseScale_.y * (1.0f - wob), baseScale_.z * (1.0f + wob)};

    // --- 撃ち落とし：自機の弾が近くにいれば両方消す ---
    for (const auto &e : scene->GetEntities()) {
      if (!e || !e->IsActive() || e->IsPendingDestroy()) continue;
      if (e->GetName() != "PlayerBullet") continue;
      auto *btr = e->GetComponent<TransformComponent>();
      if (!btr) continue;
      const RC::Vector3 d = {btr->position.x - tr->position.x,
                             btr->position.y - tr->position.y,
                             btr->position.z - tr->position.z};
      if (d.x * d.x + d.y * d.y + d.z * d.z <= shootRadius * shootRadius) {
        // 弾はプール運用（WaterBullet は非アクティブ化で回収、再利用時に reused で初期化される）
        e->SetActive(false);
        if (cam) cam->SetTag("score_add", cam->GetTagInt("score_add", 0) + 1);
        Log::Print("[InkBlobScript] Shot down.");
        OctopusDetail::SpawnInkBurst(scene, GetSceneContext(), tr->position, direction,
                                     (std::max)(baseScale_.x, 0.2f));
        Finish();
        return;
      }
    }
  }

private:
  void Finish() {
    done_ = true;
    if (Entity *self = GetEntity()) self->Destroy();
  }

  std::weak_ptr<Entity> target_;
  RC::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};
  float elapsed_ = 0.0f;
  bool done_ = false;
};

REGISTER_SCRIPT(InkBlobScript)

// ============================================================================
// タコ本体
// ============================================================================

enum class OctopusState { Wait, Approach, Windup, Recover };

class OctopusEnemyScript : public EnemyBaseScript {
public:
  // --- 索敵・位置取り ---
  float detectDistance = 45.0f;     ///< この距離に自機が来たら動き出す
  float preferredDistance = 20.0f;  ///< 自機の前方このくらいの距離に陣取る
  float sideOffset = 6.0f;          ///< 自機の正面から左右へずれる量（個体ごとに符号が変わる）
  float heightOffset = -1.5f;       ///< 自機からの高さのずれ（負で下）
  float surfaceMargin = 1.2f;       ///< 水面からこれ以上は上がらない（水面の下 m）

  // --- ジェット推進 ---
  float jetInterval = 1.1f;   ///< 1 回噴いてから次に噴くまで（秒）
  float jetImpulse = 9.0f;    ///< 1 回の噴射で得る速さ (m/s)
  float waterDrag = 1.8f;     ///< 水の抵抗（1/s）。噴射後にスーッと減速する
  float maxSpeed = 14.0f;

  // --- 墨 ---
  float fireMinDistance = 8.0f;
  float fireMaxDistance = 32.0f;
  float fireInterval = 4.0f;      ///< 吐いてから次に吐けるまで（秒）
  float firstFireDelay = 1.5f;    ///< 動き出してから最初に吐けるまで（秒）
  float windupDuration = 0.9f;    ///< ため（膨らむ）時間。この間に倒せば吐かれない
  float recoverDuration = 0.6f;   ///< 吐いたあと動けない時間
  float recoilSpeed = 6.0f;       ///< 吐いた反動で後ろへ下がる速さ
  float inkSpeed = 13.0f;
  float inkHoming = 1.1f;
  float inkAmount = 1.0f;
  int inkDamage = 0;
  float inkBlobScale = 0.7f;
  RC::Vector4 inkBlobColor = {0.05f, 0.03f, 0.08f, 1.0f};

  // --- 見た目 ---
  float squash = 0.28f;           ///< 噴射時の伸び縮み
  float windupSwell = 0.35f;      ///< ため時に膨らむ量
  RC::Vector4 windupColor = {0.55f, 0.15f, 0.75f, 1.0f}; ///< ため時に明滅する色（予兆）

protected:
  nlohmann::json Serialize() override {
    nlohmann::json j = EnemyBaseScript::Serialize();
    j["detectDistance"] = detectDistance;
    j["preferredDistance"] = preferredDistance;
    j["sideOffset"] = sideOffset;
    j["heightOffset"] = heightOffset;
    j["surfaceMargin"] = surfaceMargin;
    j["jetInterval"] = jetInterval;
    j["jetImpulse"] = jetImpulse;
    j["waterDrag"] = waterDrag;
    j["maxSpeed"] = maxSpeed;
    j["fireMinDistance"] = fireMinDistance;
    j["fireMaxDistance"] = fireMaxDistance;
    j["fireInterval"] = fireInterval;
    j["firstFireDelay"] = firstFireDelay;
    j["windupDuration"] = windupDuration;
    j["recoverDuration"] = recoverDuration;
    j["recoilSpeed"] = recoilSpeed;
    j["inkSpeed"] = inkSpeed;
    j["inkHoming"] = inkHoming;
    j["inkAmount"] = inkAmount;
    j["inkDamage"] = inkDamage;
    j["inkBlobScale"] = inkBlobScale;
    j["inkBlobColor"] = {inkBlobColor.x, inkBlobColor.y, inkBlobColor.z, inkBlobColor.w};
    j["squash"] = squash;
    j["windupSwell"] = windupSwell;
    j["windupColor"] = {windupColor.x, windupColor.y, windupColor.z, windupColor.w};
    return j;
  }

  void Deserialize(const nlohmann::json &j) override {
    EnemyBaseScript::Deserialize(j);
    auto f = [&](const char *k, float &o) { if (j.contains(k) && j[k].is_number()) o = j[k].get<float>(); };
    auto c4 = [&](const char *k, RC::Vector4 &o) {
      if (j.contains(k) && j[k].is_array() && j[k].size() == 4) {
        o = {j[k][0].get<float>(), j[k][1].get<float>(), j[k][2].get<float>(), j[k][3].get<float>()};
      }
    };
    f("detectDistance", detectDistance);
    f("preferredDistance", preferredDistance);
    f("sideOffset", sideOffset);
    f("heightOffset", heightOffset);
    f("surfaceMargin", surfaceMargin);
    f("jetInterval", jetInterval);
    f("jetImpulse", jetImpulse);
    f("waterDrag", waterDrag);
    f("maxSpeed", maxSpeed);
    f("fireMinDistance", fireMinDistance);
    f("fireMaxDistance", fireMaxDistance);
    f("fireInterval", fireInterval);
    f("firstFireDelay", firstFireDelay);
    f("windupDuration", windupDuration);
    f("recoverDuration", recoverDuration);
    f("recoilSpeed", recoilSpeed);
    f("inkSpeed", inkSpeed);
    f("inkHoming", inkHoming);
    f("inkAmount", inkAmount);
    if (j.contains("inkDamage") && j["inkDamage"].is_number()) inkDamage = j["inkDamage"].get<int>();
    f("inkBlobScale", inkBlobScale);
    c4("inkBlobColor", inkBlobColor);
    f("squash", squash);
    f("windupSwell", windupSwell);
    c4("windupColor", windupColor);
  }

  void OnCreate() override {
    EnemyBaseScript::OnCreate();
    if (!hpFromData) {
      hp = 4;
      maxHp = 4;
    }
    if (!deathDurationFromData) deathDuration = 1.6f;

    state_ = OctopusState::Wait;
    if (auto *tr = GetComponent<TransformComponent>()) {
      baseScale_ = tr->scale;
      // 左右どちらに陣取るかは生成位置で決める（同じウェーブの個体が重ならないように）
      sideSign_ = (std::fmod(std::fabs(tr->position.x * 0.37f + tr->position.z * 0.71f), 2.0f) < 1.0f) ? 1.0f : -1.0f;
      jetTimer_ = std::fmod(std::fabs(tr->position.x + tr->position.z), jetInterval); // 噴射の位相もずらす
    }
    fireCooldown_ = firstFireDelay;
  }

  void OnUpdate(float dt) override {
    EnemyBaseScript::OnUpdate(dt);
    if (dt <= 0.0f) return;

    auto *tr = GetComponent<TransformComponent>();
    Scene *scene = GetScene();
    if (!tr || !scene) return;
    time_ += dt;

    if (isDead) {
      // しぼみながら沈む
      const float k = std::clamp(1.0f - deathTimer / (std::max)(deathDuration, 0.01f), 0.0f, 1.0f);
      tr->scale = {baseScale_.x * (0.4f + 0.6f * k), baseScale_.y * (0.2f + 0.8f * k), baseScale_.z * (0.4f + 0.6f * k)};
      tr->position.y -= 1.5f * dt;
      return;
    }

    std::shared_ptr<Entity> cam = target_.lock();
    if (!cam || cam->IsPendingDestroy() || !cam->IsActive()) {
      cam = OctopusDetail::FindPlayerCamera(scene);
      target_ = cam;
      hasLastCamPos_ = false;
    }
    if (!cam) return;
    auto *camTr = cam->GetComponent<TransformComponent>();
    if (!camTr) return;

    // --- 自機の進行方向（レールの向き）。視点操作で向きが揺れても陣取る位置がぶれないよう、
    //     カメラの回転ではなく移動方向を使う。止まっているときだけ回転から取る。
    if (hasLastCamPos_) {
      const float vx = (camTr->position.x - lastCamPos_.x) / dt;
      const float vz = (camTr->position.z - lastCamPos_.z) / dt;
      const float sp = std::sqrt(vx * vx + vz * vz);
      // 墨の偏差撃ち用。フレームごとのガタつきを均す
      const float vb = (std::min)(1.0f, dt * 4.0f);
      camVel_.x += (vx - camVel_.x) * vb;
      camVel_.z += (vz - camVel_.z) * vb;
      if (sp > 0.5f) {
        const float blend = (std::min)(1.0f, dt * 3.0f);
        camFwd_.x += (vx / sp - camFwd_.x) * blend;
        camFwd_.z += (vz / sp - camFwd_.z) * blend;
      } else if (!hasFwd_) {
        camFwd_ = {std::sin(camTr->rotation.y), 0.0f, std::cos(camTr->rotation.y)};
      }
      hasFwd_ = true;
    } else if (!hasFwd_) {
      camFwd_ = {std::sin(camTr->rotation.y), 0.0f, std::cos(camTr->rotation.y)};
    }
    {
      const float l = std::sqrt(camFwd_.x * camFwd_.x + camFwd_.z * camFwd_.z);
      if (l > 1e-4f) { camFwd_.x /= l; camFwd_.z /= l; }
    }
    lastCamPos_ = camTr->position;
    hasLastCamPos_ = true;

    const RC::Vector3 camRight = {camFwd_.z, 0.0f, -camFwd_.x};
    const RC::Vector3 toCam = {camTr->position.x - tr->position.x,
                               camTr->position.y - tr->position.y,
                               camTr->position.z - tr->position.z};
    const float dist = OctopusDetail::Len(toCam);
    // 自機から見て前方にいるか（XZ）
    const float frontDot = (dist > 1e-3f) ? (-(toCam.x * camFwd_.x + toCam.z * camFwd_.z) / dist) : 0.0f;
    const bool inFront = frontDot > 0.35f;

    // 陣取る位置：自機の前方・少し横・少し下。ゆっくり左右に揺れる
    const float sway = std::sin(time_ * 0.6f) * 2.5f;
    RC::Vector3 home = {
        camTr->position.x + camFwd_.x * preferredDistance + camRight.x * (sideSign_ * sideOffset + sway),
        camTr->position.y + heightOffset + std::sin(time_ * 1.3f) * 0.8f,
        camTr->position.z + camFwd_.z * preferredDistance + camRight.z * (sideSign_ * sideOffset + sway)};
    // タコは水から出ない。自機が水上にいるときは水面すぐ下に陣取り、そこから墨を吐き上げる
    if (!waterChecked_) {
      hasWater_ = OctopusDetail::FindWaterY(scene, waterY_);
      waterChecked_ = true;
    }
    if (hasWater_) home.y = (std::min)(home.y, waterY_ - surfaceMargin);

    float swell = 0.0f; // ため中の膨らみ 0..1

    switch (state_) {
    case OctopusState::Wait:
      Drift(tr, dt);
      if (dist <= detectDistance) {
        state_ = OctopusState::Approach;
        Log::Print("[OctopusEnemyScript] Detected player -> Approach");
      }
      break;

    case OctopusState::Approach: {
      // 一定間隔で目標へ向かって噴射。噴射と噴射の間は抵抗でスーッと減速する
      jetTimer_ -= dt;
      if (jetTimer_ <= 0.0f) {
        jetTimer_ += jetInterval;
        RC::Vector3 to = {home.x - tr->position.x, home.y - tr->position.y, home.z - tr->position.z};
        const float l = OctopusDetail::Len(to);
        if (l > 0.5f) {
          // 近いほど弱く噴く（行き過ぎて振り子にならないように）
          const float k = std::clamp(l / 6.0f, 0.25f, 1.0f);
          velocity_.x += to.x / l * jetImpulse * k;
          velocity_.y += to.y / l * jetImpulse * k;
          velocity_.z += to.z / l * jetImpulse * k;
          jetPulse_ = 1.0f;
        }
      }
      Drift(tr, dt);

      fireCooldown_ -= dt;
      if (fireCooldown_ <= 0.0f && inFront && dist >= fireMinDistance && dist <= fireMaxDistance) {
        state_ = OctopusState::Windup;
        stateTimer_ = 0.0f;
        SaveOriginalColor();
      }
      break;
    }

    case OctopusState::Windup: {
      // その場で踏ん張って膨らむ（予兆）。強めの抵抗で止める
      velocity_.x *= std::exp(-6.0f * dt);
      velocity_.y *= std::exp(-6.0f * dt);
      velocity_.z *= std::exp(-6.0f * dt);
      Drift(tr, dt);

      stateTimer_ += dt;
      const float t = std::clamp(stateTimer_ / (std::max)(windupDuration, 0.01f), 0.0f, 1.0f);
      swell = t * t;
      // 明滅がだんだん速くなる。被弾フラッシュ（赤）中はそちらを優先
      if (flashTimer <= 0.0f) {
        const float blink = 0.5f + 0.5f * std::sin(stateTimer_ * (10.0f + 30.0f * t));
        ApplyColor(Lerp4(originalColor, windupColor, blink * (0.4f + 0.6f * t)));
      }

      if (stateTimer_ >= windupDuration) {
        if (flashTimer <= 0.0f) RestoreColor();
        SpitInk(scene, tr, camTr->position);
        // 反動で後ろへ
        if (dist > 1e-3f) {
          velocity_.x -= toCam.x / dist * recoilSpeed;
          velocity_.y -= toCam.y / dist * recoilSpeed;
          velocity_.z -= toCam.z / dist * recoilSpeed;
        }
        jetPulse_ = 1.0f;
        state_ = OctopusState::Recover;
        stateTimer_ = 0.0f;
        fireCooldown_ = fireInterval;
      }
      break;
    }

    case OctopusState::Recover:
      Drift(tr, dt);
      stateTimer_ += dt;
      if (stateTimer_ >= recoverDuration) {
        state_ = OctopusState::Approach;
        jetTimer_ = 0.2f;
      }
      break;
    }

    // --- 向き：常に自機の方を向く（Y 軸のみ） ---
    if (dist > 1e-3f) tr->rotation.y = std::atan2(toCam.x, toCam.z);

    // --- 見た目：噴射の伸び縮み＋ためで膨らむ ---
    jetPulse_ = (std::max)(0.0f, jetPulse_ - dt * 3.0f);
    const float p = jetPulse_ * jetPulse_ * squash;
    const float breathe = std::sin(time_ * 2.2f) * 0.04f;
    const float s = 1.0f + swell * windupSwell;
    tr->scale = {baseScale_.x * s * (1.0f - p * 0.6f + breathe),
                 baseScale_.y * s * (1.0f + p - breathe),
                 baseScale_.z * s * (1.0f - p * 0.6f + breathe)};
  }

  void TakeDamage(int damage) override {
    const bool wasWindup = (state_ == OctopusState::Windup);
    EnemyBaseScript::TakeDamage(damage);
    if (isDead) return;
    // ため中に撃たれるとひるんで中断（吐かせないチャンスを作る）
    if (wasWindup) {
      state_ = OctopusState::Recover;
      stateTimer_ = 0.0f;
      fireCooldown_ = fireInterval * 0.5f;
    }
  }

public:
  void OnImGui() override {
    EnemyBaseScript::OnImGui();
#if RC_ENABLE_IMGUI
    ImGui::Text("State: %s", StateName(state_));
    ImGui::DragFloat("Detect Dist##Octo", &detectDistance, 0.5f, 5.0f, 150.0f);
    ImGui::DragFloat("Preferred Dist##Octo", &preferredDistance, 0.5f, 3.0f, 80.0f);
    ImGui::DragFloat("Side Offset##Octo", &sideOffset, 0.1f, 0.0f, 30.0f);
    ImGui::DragFloat("Height Offset##Octo", &heightOffset, 0.1f, -20.0f, 20.0f);
    ImGui::Separator();
    ImGui::DragFloat("Jet Interval##Octo", &jetInterval, 0.05f, 0.2f, 5.0f);
    ImGui::DragFloat("Jet Impulse##Octo", &jetImpulse, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Water Drag##Octo", &waterDrag, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("Max Speed##Octo", &maxSpeed, 0.1f, 1.0f, 60.0f);
    ImGui::Separator();
    ImGui::DragFloat("Fire Min Dist##Octo", &fireMinDistance, 0.5f, 0.0f, 100.0f);
    ImGui::DragFloat("Fire Max Dist##Octo", &fireMaxDistance, 0.5f, 1.0f, 150.0f);
    ImGui::DragFloat("Fire Interval##Octo", &fireInterval, 0.1f, 0.3f, 20.0f);
    ImGui::DragFloat("Windup##Octo", &windupDuration, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("Recover##Octo", &recoverDuration, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("Ink Speed##Octo", &inkSpeed, 0.1f, 1.0f, 60.0f);
    ImGui::DragFloat("Ink Homing##Octo", &inkHoming, 0.05f, 0.0f, 10.0f);
    ImGui::SliderFloat("Ink Amount##Octo", &inkAmount, 0.1f, 1.0f);
    ImGui::DragInt("Ink Damage##Octo", &inkDamage, 1, 0, 10);
    if (ImGui::Button("Spit Now##Octo")) {
      if (auto *tr = GetComponent<TransformComponent>()) {
        if (auto cam = OctopusDetail::FindPlayerCamera(GetScene())) {
          if (auto *camTr = cam->GetComponent<TransformComponent>()) SpitInk(GetScene(), tr, camTr->position);
        }
      }
    }
#endif
  }

private:
  static const char *StateName(OctopusState s) {
    switch (s) {
    case OctopusState::Wait: return "Wait";
    case OctopusState::Approach: return "Approach";
    case OctopusState::Windup: return "Windup";
    case OctopusState::Recover: return "Recover";
    }
    return "?";
  }

  static RC::Vector4 Lerp4(const RC::Vector4 &a, const RC::Vector4 &b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
  }

  /// @brief 速度で動かして、水の抵抗で減速させる（陰的に解くので dt が大きくても発散しない）
  void Drift(TransformComponent *tr, float dt) {
    const float k = 1.0f / (1.0f + waterDrag * dt);
    velocity_.x *= k;
    velocity_.y *= k;
    velocity_.z *= k;
    const float sp = OctopusDetail::Len(velocity_);
    if (sp > maxSpeed) {
      velocity_.x *= maxSpeed / sp;
      velocity_.y *= maxSpeed / sp;
      velocity_.z *= maxSpeed / sp;
    }
    tr->position.x += velocity_.x * dt;
    tr->position.y += velocity_.y * dt;
    tr->position.z += velocity_.z * dt;
    if (hasWater_ && tr->position.y > waterY_ - surfaceMargin * 0.5f) {
      tr->position.y = waterY_ - surfaceMargin * 0.5f;
      if (velocity_.y > 0.0f) velocity_.y = 0.0f;
    }
  }

  /// @brief 墨玉を吐く
  void SpitInk(Scene *scene, const TransformComponent *tr, const RC::Vector3 &targetPos) {
    if (!scene || !tr) return;

    // 自機の未来位置を少しだけ読む（完全に読むと避けようがない）
    const float flight = OctopusDetail::Len({targetPos.x - tr->position.x,
                                             targetPos.y - tr->position.y,
                                             targetPos.z - tr->position.z}) /
                         (std::max)(inkSpeed, 1.0f);
    const RC::Vector3 aim = {targetPos.x + camVel_.x * flight * 0.5f,
                             targetPos.y,
                             targetPos.z + camVel_.z * flight * 0.5f};

    // 口（自機側の表面）から出す
    RC::Vector3 dir = {aim.x - tr->position.x, aim.y - tr->position.y, aim.z - tr->position.z};
    const float l = OctopusDetail::Len(dir);
    if (l < 1e-3f) return;
    dir = {dir.x / l, dir.y / l, dir.z / l};
    const float mouth = (std::max)({baseScale_.x, baseScale_.y, baseScale_.z}) * 0.6f;

    auto blob = scene->CreateEntity("InkBlob");
    if (!blob) return;
    auto &btr = blob->AddComponent<TransformComponent>();
    btr.position = {tr->position.x + dir.x * mouth, tr->position.y + dir.y * mouth, tr->position.z + dir.z * mouth};
    btr.scale = {inkBlobScale, inkBlobScale, inkBlobScale};

    auto &pm = blob->AddComponent<PrimitiveMeshComponent>();
    pm.type = PrimitiveType::Sphere;
    pm.color = inkBlobColor;

    auto &nsc = blob->AddComponent<NativeScriptComponent>();
    nsc.AddScript("InkBlobScript");
    if (!nsc.scripts.empty()) {
      nsc.scripts.back().pendingData = {
          {"speed", inkSpeed},
          {"homingRate", inkHoming},
          {"inkAmount", inkAmount},
          {"damage", inkDamage},
          {"dir", {dir.x, dir.y, dir.z}},
      };
    }
    nsc.SetScene(scene);
    if (GetSceneContext()) nsc.SetSceneContext(GetSceneContext());

    scene->InitDynamicEntityRuntime(*blob);

    // 生成直後に PrimitiveMesh の Transform を合わせて原点でのチラつきを防ぐ
    if (auto *newPm = blob->GetComponent<PrimitiveMeshComponent>()) {
      if (newPm->meshHandle >= 0) {
        if (auto *pmTr = RC::GetPrimitiveMeshTransformPtr(newPm->meshHandle)) {
          pmTr->scale = btr.scale;
          pmTr->rotation = btr.rotation;
          pmTr->translation = btr.position;
        }
      }
    }
    Log::Print("[OctopusEnemyScript] Spit ink!");
  }

  OctopusState state_ = OctopusState::Wait;
  std::weak_ptr<Entity> target_;
  RC::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};
  RC::Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
  RC::Vector3 camFwd_ = {0.0f, 0.0f, 1.0f};
  RC::Vector3 camVel_ = {0.0f, 0.0f, 0.0f};
  RC::Vector3 lastCamPos_ = {0.0f, 0.0f, 0.0f};
  bool hasLastCamPos_ = false;
  bool hasFwd_ = false;
  float sideSign_ = 1.0f;
  float time_ = 0.0f;
  float jetTimer_ = 0.0f;
  float jetPulse_ = 0.0f;
  float stateTimer_ = 0.0f;
  float fireCooldown_ = 0.0f;
  bool waterChecked_ = false;
  bool hasWater_ = false;
  float waterY_ = 0.0f;
};

REGISTER_SCRIPT(OctopusEnemyScript)
