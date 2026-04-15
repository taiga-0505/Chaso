#include "Coin.h"
#include <numbers>

void Coin::Init(int modelIndex, SceneContext &ctx) {
  ctx_ = &ctx;
  model_ = modelIndex;
  transform_ = RC::GetModelTransformPtr(model_);
  RC::SetModelLightingMode(model_, None);
  transform_->translation = {0.0f, 0.0f, 0.0f};
  transform_->scale = {0.5f, 0.5f, 0.5f};
  transform_->rotation.y = 0.0f;
}

void Coin::Update() {
  if (!transform_)
    return;

  constexpr float dt = 1.0f / 60.0f;

  if (!collected_) {
    // 通常：回転
    transform_->rotation.y += 2.5f * (std::numbers::pi_v<float> / 180.0f);
    if (transform_->rotation.y > std::numbers::pi_v<float> * 2.0f) {
      transform_->rotation.y -= std::numbers::pi_v<float> * 2.0f;
    }
    return;
  }

  // 回収演出：上に上がりながら回転 → 一定時間で消える
  collectedT_ += dt;

  transform_->translation.y =
      collectedStartPos_.y + collectedT_ * 3.0f; // 上昇スピード
  transform_->rotation.y += 10.0f * (std::numbers::pi_v<float> / 180.0f);

  if (collectedT_ >= 0.25f) { // 0.25秒くらいで消す（好みで）
    isAlive_ = false;
  }
}

void Coin::Draw() { 
    if (isAlive_)
    RC::DrawModel(model_); 
}

void Coin::GetCoin() {
  if (!transform_)
    return;
  if (collected_ || !isAlive_)
    return;

  collected_ = true;
  collectedT_ = 0.0f;
  collectedStartPos_ = transform_->translation;
}

void Coin::SetWorldPos(const RC::Vector3 &pos) {
  if (!transform_)
    return;
  transform_->translation = pos;
}

RC::Vector3 Coin::GetWorldPos() const {
  return transform_ ? transform_->translation : RC::Vector3{};
}
