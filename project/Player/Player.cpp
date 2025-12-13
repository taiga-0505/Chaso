#include "Player.h"
#include "Input/Input.h"
#include "RenderCommon.h"

void Player::Init(int modelIndex, SceneContext &ctx) {

  ctx_ = &ctx;

  model_ = modelIndex;
  transform_ = RC::GetModelTransformPtr(model_);

  RC::SetModelLightingMode(model_, None);

  transform_->translation = {1.0f, 1.0f, 0.0f};
}

void Player::Update() {

#if RC_ENABLE_IMGUI
  RC::DrawImGui3D(model_, "Player");
#endif

  Move();
  Jump();
}

void Player::Draw() { RC::DrawModel(model_); }

void Player::Move() {

  if (ctx_->input->IsKeyPressed(DIK_A)) {
    transform_->translation.x -= moveSpeed_;
  }
  if (ctx_->input->IsKeyPressed(DIK_D)) {
    transform_->translation.x += moveSpeed_;
  }

  if (transform_->translation.y > 1.0f) {
    transform_->translation.y -= gravity_;
  }

  // transform_->translation -= velocity_;
}

void Player::Jump() {
  if (transform_->translation.y > 1.0f) {
    return;
  }

  if (ctx_->input->IsKeyPressed(DIK_SPACE)) {
    transform_->translation.y += jumpStrength_;
  }
}
