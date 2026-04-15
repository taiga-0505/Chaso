#pragma once
#include "Math/MathTypes.h"
#include "SceneManager.h"
#include "struct.h"
#include <numbers>

class MapChipField;

enum class LRDirection {
  kRight,
  kLeft,
};

// ==============
// Utility Functions
// ==============
// ======= Easing =======
inline float easeInOutSine(float x) {
  return -(std::cos(std::numbers::pi_v<float> * x) - 1.0f) / 2.0f;
}

// ======= Interpolation =======
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

class Player {
public:
  // ==============
  // Public API
  // ==============
  // ======= Lifecycle =======
  void Init(int modelIndex, SceneContext &ctx);
  ~Player();
  void Update();
  void Draw();

  // ======= Scene Integration =======
  void SetMap(MapChipField *map) { map_ = map; }
  void SetPosition(const RC::Vector3 &pos) {
    if (transform_) {
      transform_->translation = pos;
    }
  }

  // ======= Gameplay State =======
  void GetCoin(int add = 1);
  int CoinCount() const { return coinCount_; }
  RC::Vector3 GetWorldPos() const;

private:
  // ==============
  // Simulation Flow
  // ==============
  // ======= Frame Steps =======
  void TickInput_();
  void SimulateAndCollide_();
  void ModelRotate();

  // ======= Direction Control =======
  void BeginTurn_(LRDirection newDir);
  static float FacingAngleForDirection_(LRDirection dir);

  // ==============
  // Collision Helpers
  // ==============
  // ======= Shapes =======
  struct Rect {
    float left;
    float right;
    float bottom;
    float top;
  };
  Rect MakeAabb_(const RC::Vector3 &pos) const;
  static bool Overlap_(const Rect &a, const Rect &b);

  // ======= Axis Resolution =======
  void ResolveX_(RC::Vector3 &pos);
  void ResolveY_(RC::Vector3 &pos);

private:
  // ==============
  // State
  // ==============
  // ======= External References =======
  SceneContext *ctx_ = nullptr;
  MapChipField *map_ = nullptr;
  // oooooooooo モデル ID と Transform
  int model_ = -1;
  Transform *transform_ = nullptr;

  // ======= Motion =======
  RC::Vector3 velocity_ = {};
  // oooooooooo 移動チューニング
  float moveSpeed_ = 0.15f;
  float dashMultiplier_ = 2.0f;
  float jumpStrength_ = 0.55f;
  float gravity_ = 0.03f;
  float wallJumpHorizontalSpeed_ = 0.5f;

  // ======= Collision Shape =======
  float halfW_ = 0.5f;
  float halfH_ = 0.45f;
  float skin_ = 0.001f;

  // ======= Contact Flags =======
  bool onGround_ = false;
  bool onWallLeft_ = false;
  bool onWallRight_ = false;
  // oooooooooo 押しっぱ対策
  bool prevSpace_ = false;

  // ======= Facing =======
  LRDirection lrDirection_ = LRDirection::kRight;
  float turnFirstRotation_ = 0.0f;
  float turnTimer_ = 0.0f;
  static inline const float kTimeTurn = 0.45f;

  // ======= Collectibles =======
  int coinCount_ = 0;
};
