#include "Goal.h"
#include <cmath>
#include <numbers>

void Goal::Init(int modelIndex) {
  model_ = modelIndex;
  transform_ = RC::GetModelTransformPtr(model_);
  RC::SetModelLightingMode(model_, None);

  if (!transform_)
    return;

  transform_->translation = {0, 0, 0};
  transform_->scale = {0.8f, 0.8f, 0.8f};
  transform_->rotation = {0, 0, 0};

  basePos_ = transform_->translation;
}

void Goal::Update() {
  if (!transform_)
    return;

  constexpr float dt = 1.0f / 60.0f;
  t_ += dt;

  // ふわふわ + 回転
  transform_->rotation.y += 1.5f * (std::numbers::pi_v<float> / 180.0f);
  const float bob = std::sin(t_ * 2.5f) * 0.1f;

  transform_->translation = basePos_;
  transform_->translation.y += bob;
}

void Goal::Draw() {
  if (!transform_)
    return;
  RC::DrawModelNoCull(model_);
}

void Goal::SetWorldPos(const RC::Vector3 &pos) {
  if (!transform_)
    return;
  transform_->translation = pos;
  basePos_ = pos;
}

RC::Vector3 Goal::GetWorldPos() const {
  return transform_ ? transform_->translation : RC::Vector3{};
}

void Goal::Reach() { reached_ = true; }
