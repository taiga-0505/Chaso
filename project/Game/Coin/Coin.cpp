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
  // 回転アニメーション
  transform_->rotation.y +=
      2.5f * (std::numbers::pi_v<float> / 180.0f);
  if (transform_->rotation.y > std::numbers::pi_v<float> * 2.0f) {
    transform_->rotation.y -= std::numbers::pi_v<float> * 2.0f;
  }
}

void Coin::Draw() { 
    if (isAlive_)
    RC::DrawModel(model_); 
}

void Coin::GetCoin() {
    // コインが取られたとき回転しながら上に上がり消える
    if (!transform_)
      return;
    transform_->translation.y += 0.1f; // 上に上がる
    transform_->rotation.y +=
        5.0f * (std::numbers::pi_v<float> / 180); // 5度ずつ回転

    isAlive_ = false; // コインが取られたフラグを立てる
}

void Coin::SetWorldPos(const RC::Vector3 &pos) {
  if (!transform_)
    return;
  transform_->translation = pos;
}
