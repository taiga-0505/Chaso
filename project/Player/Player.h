#pragma once
#include "Math/MathTypes.h"
#include "SceneManager.h"
#include "struct.h"

class MapChipField;

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

  // AABB(2D) 用
  struct Rect {
    float left, right, bottom, top;
  };
  Rect MakeAabb_(const RC::Vector3 &pos) const;
  static bool Overlap_(const Rect &a, const Rect &b);

  void ResolveX_(RC::Vector3 &pos);
  void ResolveY_(RC::Vector3 &pos);

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
};
