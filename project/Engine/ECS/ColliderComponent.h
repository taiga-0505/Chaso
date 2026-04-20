#pragma once

// ============================================================================
// ColliderComponent
// ----------------------------------------------------------------------------
// 当たり判定コンポーネント（将来拡張の準備）。
// Shape の種類と AABB / Sphere パラメータを保持する。
// ============================================================================

#include "IComponent.h"
#include "Math/MathTypes.h"
#include "struct.h" // AABB

class ColliderComponent : public IComponent {
public:
  /// コライダーの形状
  enum class Shape {
    AABB,
    Sphere,
    Capsule,
  };

  Shape shape = Shape::AABB;

  /// AABB パラメータ
  AABB aabb;

  /// Sphere パラメータ
  RC::Vector3 center = {0.0f, 0.0f, 0.0f};
  float radius = 1.0f;

  /// 衝突レイヤー（ビットマスク）
  uint32_t layer = 0xFFFFFFFF;

  /// トリガーモード（true: 物理応答なし、イベントのみ）
  bool isTrigger = false;
};
