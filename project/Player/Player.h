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

// easeInOutSine関数の実装
inline float easeInOutSine(float x) {
  return -(std::cos(std::numbers::pi_v<float> * x) - 1.0f) / 2.0f;
}

// 線形補間関数
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

class Player {
public:
  void Init(int modelIndex, SceneContext &ctx);
  void Update();
  void Draw();

  void SetMap(MapChipField *map) { map_ = map; }
  void SetPosition(const RC::Vector3 &pos) {
    if (transform_) {
      transform_->translation = pos;
    }
  }

private:
  void TickInput_();
  void SimulateAndCollide_();
  void BeginTurn_(LRDirection newDir);

  // AABB(2D) 用
  struct Rect {
    float left, right, bottom, top;
  };
  Rect MakeAabb_(const RC::Vector3 &pos) const;
  static bool Overlap_(const Rect &a, const Rect &b);

  void ResolveX_(RC::Vector3 &pos);
  void ResolveY_(RC::Vector3 &pos);

  void ModelRotate();

  static float NormalizeTurnAngle_(float angle);
  static float FacingAngleForDirection_(LRDirection dir);

private:
  SceneContext *ctx_ = nullptr;
  MapChipField *map_ = nullptr;

  int model_ = -1;
  Transform *transform_ = nullptr;

  RC::Vector3 velocity_ = {};

  float moveSpeed_ = 0.15f;    // 好みで
  float jumpStrength_ = 0.55f; // “速度”として扱う（毎フレーム加算じゃなく）
  float gravity_ = 0.03f;      // 毎フレーム落ちる量（速度に加算）

  // 当たり判定サイズ（blockSize=1想定。合わなければ調整）
  float halfW_ = 0.5f;
  float halfH_ = 0.45f;

  float skin_ = 0.001f; // めり込み防止の微小マージン

  bool onGround_ = false;

  // “押しっぱジャンプ”を避ける用（InputにTriggerが無い前提）
  bool prevSpace_ = false;

  // 移動方向
  LRDirection lrDirection_ = LRDirection::kRight;
  float turnFirstRotation_ = 0.0f; // 初回の向き
  float turnTimer_ = 0.0f;         // 向き変更のタイマー

  static inline const float kTimeTurn = 0.45f; // 向き変更のスピード
};
