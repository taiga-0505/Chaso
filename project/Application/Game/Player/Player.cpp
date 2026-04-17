#include "Player.h"
#include "Input/Input.h"
#include "MapChipField/MapChipField.h"
#include "RenderCommon.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// ==============
// Public API
// ==============
// ======= Lifecycle =======
void Player::Init(int modelIndex, SceneContext &ctx) {
  ctx_ = &ctx;

  model_ = modelIndex;
  transform_ = RC::GetModelTransformPtr(model_);

  RC::SetModelLightingMode(model_, None);

  transform_->translation = {1.0f, 2.0f,
                             0.0f}; // 初期は少し上に置くと分かりやすい

  transform_->rotation.y = std::numbers::pi_v<float> / -2.0f;
  velocity_ = {};
  onGround_ = false;
  prevSpace_ = false;
}

Player::~Player() {
  if (model_ >= 0) {
    model_ = -1;
  }
}

void Player::Update() {
#if RC_ENABLE_IMGUI
  RC::DrawImGui3D(model_, "Player");
#endif

  TickInput_();
  SimulateAndCollide_();
  ModelRotate();
}

void Player::Draw() { RC::DrawModel(model_); }

// ======= Gameplay State =======
RC::Vector3 Player::GetWorldPos() const {
  return transform_ ? transform_->translation : RC::Vector3{};
}

void Player::GetCoin(int add) {
  coinCount_ += add;
#if RC_ENABLE_IMGUI
  // 好きならここでログ出したりしてもOK
#endif
}

// ==============
// Simulation Flow
// ==============
// ======= Input Phase =======
void Player::TickInput_() {
  float vx = 0.0f;

  const bool left = ctx_->input->IsKeyPressed(DIK_A);
  const bool right = ctx_->input->IsKeyPressed(DIK_D);
  const bool dash = ctx_->input->IsKeyPressed(DIK_LSHIFT) ||
                    ctx_->input->IsKeyPressed(DIK_RSHIFT);
  const float currentMoveSpeed =
      dash ? moveSpeed_ * dashMultiplier_ : moveSpeed_;

  if (left) {
    vx -= currentMoveSpeed;
  }
  if (right) {
    vx += currentMoveSpeed;
  }

  // 入力が片側だけある時に「向き変更」を開始
  if (vx > 0.0f) {
    BeginTurn_(LRDirection::kRight);
  } else if (vx < 0.0f) {
    BeginTurn_(LRDirection::kLeft);
  }
  // vx == 0 の時は向き維持

  velocity_.x = vx;

  // ジャンプ（そのまま）
  bool space = ctx_->input->IsKeyPressed(DIK_SPACE);
  bool jumpPressed = space && !prevSpace_;
  prevSpace_ = space;

  if (jumpPressed) {
    if (onGround_) {
      velocity_.y = jumpStrength_;
      onGround_ = false;
    } else if (onWallLeft_ || onWallRight_) {
      velocity_.y = jumpStrength_;
      if (onWallLeft_) {
        velocity_.x = wallJumpHorizontalSpeed_;
        BeginTurn_(LRDirection::kRight);
      } else {
        velocity_.x = -wallJumpHorizontalSpeed_;
        BeginTurn_(LRDirection::kLeft);
      }
    }
  }
}

// ======= Physics Phase =======
void Player::SimulateAndCollide_() {
  if (!transform_) {
    return;
  }

  // map が無いなら、とりあえず今まで通り落とす…ではなく、衝突なしで自由落下
  // （map渡したらちゃんと床で止まる）
  // 重力（上が+Y、下が-Y の想定）
  velocity_.y -= gravity_;

  RC::Vector3 pos = transform_->translation;

  onWallLeft_ = false;
  onWallRight_ = false;

  // X→解決
  pos.x += velocity_.x;
  ResolveX_(pos);

  // Y→解決
  pos.y += velocity_.y;
  ResolveY_(pos);

  transform_->translation = pos;
}

// ==============
// Collision Helpers
// ==============
// ======= Bounds =======
Player::Rect Player::MakeAabb_(const RC::Vector3 &pos) const {
  return Rect{pos.x - halfW_, pos.x + halfW_, pos.y - halfH_, pos.y + halfH_};
}

bool Player::Overlap_(const Rect &a, const Rect &b) {
  if (a.right <= b.left) {
    return false;
  }
  if (a.left >= b.right) {
    return false;
  }
  if (a.top <= b.bottom) {
    return false;
  }
  if (a.bottom >= b.top) {
    return false;
  }
  return true;
}

// ======= Axis Resolution =======
void Player::ResolveX_(RC::Vector3 &pos) {
  if (!map_) {
    return;
  }

  Rect aabb = MakeAabb_(pos);
  auto tiles = map_->CollectTilesOverlapping(
      MapChipField::Rect{aabb.left, aabb.right, aabb.bottom, aabb.top},
      MapChipField::kSolid);

  if (tiles.empty()) {
    return;
  }

  if (velocity_.x > 0.0f) {
    float bestX = pos.x;
    bool hit = false;

    for (auto idx : tiles) {
      auto tr = map_->RectAt(idx);
      Rect tileR{tr.left, tr.right, tr.bottom, tr.top};
      if (!Overlap_(aabb, tileR)) {
        continue;
      }

      bestX = (std::min)(bestX, tileR.left - halfW_ - skin_);
      hit = true;
    }

    if (hit) {
      pos.x = bestX;
      velocity_.x = 0.0f;
      onWallRight_ = true;
    }
  } else if (velocity_.x < 0.0f) {
    float bestX = pos.x;
    bool hit = false;

    for (auto idx : tiles) {
      auto tr = map_->RectAt(idx);
      Rect tileR{tr.left, tr.right, tr.bottom, tr.top};
      if (!Overlap_(aabb, tileR)) {
        continue;
      }

      bestX = (std::max)(bestX, tileR.right + halfW_ + skin_);
      hit = true;
    }

    if (hit) {
      pos.x = bestX;
      velocity_.x = 0.0f;
      onWallLeft_ = true;
    }
  }
}

void Player::ResolveY_(RC::Vector3 &pos) {
  onGround_ = false;
  if (!map_) {
    return;
  }

  Rect aabb = MakeAabb_(pos);
  auto tiles = map_->CollectTilesOverlapping(
      MapChipField::Rect{aabb.left, aabb.right, aabb.bottom, aabb.top},
      MapChipField::kSolid);

  if (tiles.empty()) {
    return;
  }

  if (velocity_.y > 0.0f) {
    // 上昇：天井に当たったら tile.bottom へ
    for (auto idx : tiles) {
      auto tr = map_->RectAt(idx);
      Rect tileR{tr.left, tr.right, tr.bottom, tr.top};
      if (!Overlap_(aabb, tileR)) {
        continue;
      }

      pos.y = tileR.bottom - halfH_ - skin_;
      velocity_.y = 0.0f;
      aabb = MakeAabb_(pos);
    }
  } else if (velocity_.y < 0.0f) {
    // 落下：床に当たったら tile.top へ
    for (auto idx : tiles) {
      auto tr = map_->RectAt(idx);
      Rect tileR{tr.left, tr.right, tr.bottom, tr.top};
      if (!Overlap_(aabb, tileR)) {
        continue;
      }

      pos.y = tileR.top + halfH_ + skin_;
      velocity_.y = 0.0f;
      onGround_ = true;
      aabb = MakeAabb_(pos);
    }
  }
}

// ==============
// Orientation
// ==============
// ======= Rotation Helpers =======
float Player::FacingAngleForDirection_(LRDirection dir) {
  switch (dir) {
  case LRDirection::kRight:
    return -std::numbers::pi_v<float> / 2.0f;
  case LRDirection::kLeft:
    return std::numbers::pi_v<float> / 2.0f;
  default:
    return -std::numbers::pi_v<float> / 2.0f;
  }
}

void Player::ModelRotate() {
  if (turnTimer_ > 0.0f) {
    turnTimer_ -= 1.0f / 60.0f;

    float destinationRotationYTable[] = {
        std::numbers::pi_v<float> / -2.0f,        // 右方向
        std::numbers::pi_v<float> * 3.0f / -2.0f, // 左方向
    };

    float destinationRotationY =
        destinationRotationYTable[static_cast<uint32_t>(lrDirection_)];
    float t = 1.0f - std::clamp(turnTimer_ / kTimeTurn, 0.0f, 1.0f);
    float easedT = easeInOutSine(t);
    transform_->rotation.y =
        lerp(turnFirstRotation_, destinationRotationY, easedT);
  } else {
    transform_->rotation.y = FacingAngleForDirection_(lrDirection_);
  }
}

void Player::BeginTurn_(LRDirection newDir) {
  if (lrDirection_ == newDir) {
    return; // 同じ向きなら何もしない
  }

  lrDirection_ = newDir;

  // 今の角度を「開始角」にして、ここからイージングで回す
  if (transform_) {
    turnFirstRotation_ = transform_->rotation.y;
  } else {
    turnFirstRotation_ = 0.0f;
  }

  turnTimer_ = kTimeTurn; // 回転開始
}
