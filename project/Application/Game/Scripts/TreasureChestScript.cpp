#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/TransformComponent.h"
#include "ECS/ColliderComponent.h"
#include "ECS/ModelRendererComponent.h"
#include "ECS/PrimitiveMeshComponent.h"
#include "ECS/GPUParticleComponent.h"
#include "ECS/NativeScriptComponent.h"
#include "ECS/CameraComponent.h"
#include "Particle/GPUParticle.h"
#include "RenderCommon.h"
#include "Scene.h"
#include "Common/Log/Log.h"
#include "Application/Game/Framework/GameSession.h"

#include <cmath>
#include <format>
#include <memory>
#include <string>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

// =====================================================================
// 宝箱：弾を何発か当てると開いてポイント（スコア）が手に入る
// =====================================================================
// 敵（EnemyBaseScript）とは別物として扱う。理由は 2 つ。
//   ・敵として数えると「全滅でクリア」の判定や敵 HP バーの集計に混ざってしまう
//   ・撃破数ではなく「宝箱取得数」としてリザルトに出したい
// そのためタグは `is_enemy` ではなく `is_item` を立て、
// WaterBullet 側は `is_item` を当たり判定の対象に含めている。
//
// 弾からダメージを受ける仕組みは敵と同じ（疎結合のタグ方式）:
//   WaterBullet が `pending_damage` を積む → ここが OnUpdate で拾って TakeDamage
//
// 見た目は ModelRendererComponent でも PrimitiveMeshComponent でもよい。
// どちらも無いときは、見えないと困るので茶色い Box を仮で付ける。
//
// JSON（scriptDataList）で指定できる値:
//   "maxHp"        : 何発当てれば開くか（既定 3）。"hp" だけ書いてもよい
//   "points"       : 開いたときに加算されるスコア（既定 100）
//   "bobAmplitude" : ふわふわ上下する幅(m)。0 で静止（既定 0.2）
//   "bobSpeed"     : 上下の速さ（既定 1.5）
//   "spinSpeedDeg" : Y 軸回転の速さ(度/秒)。0 で回さない（既定 0）
//   "openDuration" : 開いてから実体を消すまでの秒数（既定 1.2）

/// @brief 宝箱。弾を当てると HP が減り、0 になると開いてポイントを獲得する
class TreasureChestScript : public ScriptableEntity {
public:
  int hp = 3;
  int maxHp = 3;
  /// @brief 開いたときにスコアへ加算する点数
  int points = 100;

  // --- 待機中の演出 ---
  float bobAmplitude = 0.2f;
  float bobSpeed = 1.5f;
  float spinSpeedDeg = 0.0f;

  // --- 開いたあとの演出 ---
  /// @brief 開いてから実体を消すまでの秒数
  float openDuration = 1.2f;
  /// @brief 開いた瞬間に飛び上がる高さ(m)
  float popHeight = 1.0f;

  bool isOpened = false;

protected:
  nlohmann::json Serialize() override {
    nlohmann::json j;
    j["hp"] = hp;
    j["maxHp"] = maxHp;
    j["points"] = points;
    j["bobAmplitude"] = bobAmplitude;
    j["bobSpeed"] = bobSpeed;
    j["spinSpeedDeg"] = spinSpeedDeg;
    j["openDuration"] = openDuration;
    j["popHeight"] = popHeight;
    return j;
  }

  void Deserialize(const nlohmann::json& j) override {
    // EnemyBaseScript と同じ流儀: "maxHp" だけ書けば hp も満タンに揃える
    if (j.contains("maxHp")) {
      maxHp = j["maxHp"].get<int>();
      hp = maxHp;
    }
    if (j.contains("hp")) {
      hp = j["hp"].get<int>();
      if (!j.contains("maxHp")) maxHp = hp;
    }
    if (j.contains("points")) points = j["points"].get<int>();
    if (j.contains("bobAmplitude")) bobAmplitude = j["bobAmplitude"].get<float>();
    if (j.contains("bobSpeed")) bobSpeed = j["bobSpeed"].get<float>();
    if (j.contains("spinSpeedDeg")) spinSpeedDeg = j["spinSpeedDeg"].get<float>();
    if (j.contains("openDuration")) openDuration = j["openDuration"].get<float>();
    if (j.contains("popHeight")) popHeight = j["popHeight"].get<float>();
    if (maxHp < 1) maxHp = 1;
    if (hp < 1) hp = maxHp;
  }

  void OnCreate() override {
    Entity* self = GetEntity();
    if (!self) return;

    // 弾の当たり判定対象にする（敵とは別のタグ）
    self->SetTag("is_item", 1);

    // コライダーがなければフェールセーフとして追加
    if (!self->HasComponent<ColliderComponent>()) {
      auto* col = &self->AddComponent<ColliderComponent>();
      col->shape = ColliderComponent::Shape::Sphere;
      col->radius = 1.0f; // スケール依存
      col->isTrigger = false;
    }

    // 見た目が何も無ければ仮の箱を付ける（モデル差し替え前でも動作確認できるように）
    if (!self->HasComponent<ModelRendererComponent>() && !self->HasComponent<PrimitiveMeshComponent>()) {
      auto* pm = &self->AddComponent<PrimitiveMeshComponent>();
      pm->type = PrimitiveType::Box;
      pm->meshHandle = RC::GenerateBox(1.0f, 1.0f, 1.0f);
      if (pm->meshHandle >= 0) {
        if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
          mat->color = {0.55f, 0.35f, 0.15f, 1.0f};
        }
      }
    }

    // 基準位置は最初の OnUpdate で取る（JSON の Transform が確実に入ったあと）
    hasBase_ = false;
    isOpened = false;
    openTimer_ = 0.0f;
    bobTime_ = 0.0f;
  }

  void OnUpdate(float deltaTime) override {
    Entity* self = GetEntity();
    if (!self) return;

    // 弾が積んだダメージを拾う
    int dmg = self->GetTagInt("pending_damage", 0);
    if (dmg > 0) {
      self->ClearTag("pending_damage");
      TakeDamage(dmg);
    }

    // 被弾フラッシュを戻す
    if (flashTimer_ > 0.0f) {
      flashTimer_ -= deltaTime;
      if (flashTimer_ <= 0.0f) RestoreColor();
    }

    auto* tr = self->GetComponent<TransformComponent>();
    if (!tr) return;
    if (!hasBase_) {
      basePosition_ = tr->position;
      baseScale_ = tr->scale;
      baseRotation_ = tr->rotation;
      hasBase_ = true;
    }

    if (!isOpened) {
      // 待機中: ふわふわ上下 ＋ 任意で回転
      bobTime_ += deltaTime;
      tr->position = basePosition_;
      tr->position.y += std::sin(bobTime_ * bobSpeed) * bobAmplitude;
      if (spinSpeedDeg != 0.0f) {
        tr->rotation.y = baseRotation_.y + bobTime_ * spinSpeedDeg * (3.14159265f / 180.0f);
      }
      // UI 用に HP を公開
      self->SetTag("current_hp", hp);
      self->SetTag("max_hp", maxHp);
      return;
    }

    // 開いたあと: ぴょんと跳ねて、くるっと回りながら縮んで消える
    openTimer_ += deltaTime;
    const float t = (openDuration > 0.0f) ? (openTimer_ / openDuration) : 1.0f;
    const float clamped = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    // 放物線で跳ねる（0 → 頂点 → 0）
    const float hop = 4.0f * clamped * (1.0f - clamped) * popHeight;
    tr->position = basePosition_;
    tr->position.y += hop;
    tr->rotation.y = baseRotation_.y + clamped * 6.28318530718f;
    // 後半で縮む
    const float shrink = (clamped < 0.5f) ? 1.0f : (1.0f - (clamped - 0.5f) * 2.0f);
    const float s = (shrink < 0.0f) ? 0.0f : shrink;
    tr->scale = {baseScale_.x * s, baseScale_.y * s, baseScale_.z * s};

    if (openTimer_ >= openDuration) {
      self->Destroy();
    }
  }

public:
  void OnImGui() override {
#if RC_ENABLE_IMGUI
    ImGui::DragInt("HP##TreasureChest", &hp, 1, 0, maxHp);
    ImGui::DragInt("Max HP##TreasureChest", &maxHp, 1, 1, 100);
    ImGui::DragInt("Points##TreasureChest", &points, 10, 0, 100000);
    ImGui::DragFloat("Bob Amplitude##TreasureChest", &bobAmplitude, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Bob Speed##TreasureChest", &bobSpeed, 0.1f, 0.0f, 20.0f);
    ImGui::DragFloat("Spin Speed (deg/s)##TreasureChest", &spinSpeedDeg, 1.0f, -720.0f, 720.0f);
    ImGui::DragFloat("Open Duration##TreasureChest", &openDuration, 0.05f, 0.1f, 10.0f);
    ImGui::DragFloat("Pop Height##TreasureChest", &popHeight, 0.05f, 0.0f, 10.0f);
    ImGui::Text("Opened: %s", isOpened ? "yes" : "no");
#endif
  }

  void TakeDamage(int damage) {
    if (isOpened) return;
    hp -= damage;
    Log::Print(std::format("[TreasureChest] {} took {} damage. HP {}/{}", GetEntity()->GetName(), damage,
                           hp, maxHp));

    // 敵と同じく白っぽく光らせて手応えを出す
    SaveOriginalColor();
    ApplyColor({1.0f, 1.0f, 0.6f, 1.0f});
    flashTimer_ = 0.15f;

    if (hp <= 0) {
      hp = 0;
      Open();
    }
  }

private:
  RC::Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};
  RC::Vector3 baseScale_ = {1.0f, 1.0f, 1.0f};
  RC::Vector3 baseRotation_ = {0.0f, 0.0f, 0.0f};
  bool hasBase_ = false;
  float bobTime_ = 0.0f;
  float openTimer_ = 0.0f;
  float flashTimer_ = 0.0f;
  RC::Vector4 originalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
  bool hasSavedColor_ = false;

  /// @brief 開く。ポイントを加算し、以後は弾が当たらないようにする
  void Open() {
    if (isOpened) return;
    isOpened = true;
    openTimer_ = 0.0f;
    RestoreColor();

    Entity* self = GetEntity();
    if (!self) return;

    // 以後は弾が当たらないように
    self->ClearTag("is_item");
    self->ClearTag("current_hp");
    self->ClearTag("max_hp");
    if (auto* col = self->GetComponent<ColliderComponent>()) col->SetEnabled(false);

    // スコア加算: プレイヤー（RailShooterController が載っている Entity）の score_add へ積む
    AddScore(points);

    // リザルト用の取得数
    GameSession::Get().AddChestCollected();

    Log::Print(std::format("[TreasureChest] {} opened! +{} points", self->GetName(), points));

    // 開いた瞬間の泡エフェクト（敵撃破と同じ GPU 泡）
    SpawnBubbles();
  }

  void AddScore(int amount) {
    if (amount <= 0) return;
    Scene* scene = GetScene();
    if (!scene) return;

    std::shared_ptr<Entity> target = cachedPlayer_.lock();
    if (!target || target->IsPendingDestroy() || !target->IsActive()) {
      target = nullptr;
      std::shared_ptr<Entity> cameraFallback = nullptr;
      for (auto& e : scene->GetEntities()) {
        if (!e || !e->IsActive() || e->IsPendingDestroy()) continue;
        if (e->GetTagInt("is_player", 0) == 1) {
          target = e;
          break;
        }
        if (!cameraFallback && e->HasComponent<CameraComponent>()) cameraFallback = e;
      }
      if (!target) target = cameraFallback;
      cachedPlayer_ = target;
    }
    if (target) {
      int scoreAdd = target->GetTagInt("score_add", 0);
      target->SetTag("score_add", scoreAdd + amount);
    }
  }
  std::weak_ptr<Entity> cachedPlayer_;

  void SpawnBubbles() {
    Scene* scene = GetScene();
    Entity* self = GetEntity();
    if (!scene || !self) return;

    auto emitter = scene->CreateEntity("GPUBubbleEmitter");
    auto* tr = &emitter->AddComponent<TransformComponent>();
    if (auto* myTr = self->GetComponent<TransformComponent>()) tr->position = myTr->position;
    auto* gpu = &emitter->AddComponent<GPUParticleComponent>();
    if (gpu->particleSystem) gpu->particleSystem->SetPipelinePrefix("gpu_particle_bubble");
    auto* nsc = &emitter->AddComponent<NativeScriptComponent>();
    nsc->AddScript("GPUBubbleEmitter");
    nsc->SetScene(scene);
    if (GetSceneContext()) nsc->SetSceneContext(GetSceneContext());
    scene->InitDynamicEntityRuntime(*emitter);
  }

  // --- 色の保存／適用（EnemyBaseScript と同じ方式）---
  void SaveOriginalColor() {
    if (hasSavedColor_) return;
    Entity* self = GetEntity();
    if (!self) return;
    if (auto* mr = self->GetComponent<ModelRendererComponent>()) {
      originalColor_ = mr->color;
      hasSavedColor_ = true;
    } else if (auto* pm = self->GetComponent<PrimitiveMeshComponent>()) {
      if (pm->meshHandle >= 0) {
        if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) {
          originalColor_ = mat->color;
          hasSavedColor_ = true;
        }
      }
    }
  }

  void ApplyColor(const RC::Vector4& color) {
    Entity* self = GetEntity();
    if (!self) return;
    if (auto* mr = self->GetComponent<ModelRendererComponent>()) {
      mr->color = color;
    } else if (auto* pm = self->GetComponent<PrimitiveMeshComponent>()) {
      if (pm->meshHandle >= 0) {
        if (auto* mat = RC::GetPrimitiveMeshMaterialPtr(pm->meshHandle)) mat->color = color;
      }
    }
  }

  void RestoreColor() {
    if (hasSavedColor_) ApplyColor(originalColor_);
  }
};

REGISTER_SCRIPT(TreasureChestScript)
