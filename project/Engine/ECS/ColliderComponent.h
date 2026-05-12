#pragma once
#include "IComponent.h"
#include "Math/MathTypes.h"
#include "struct.h" // AABB

/// @brief 当たり判定（衝突）情報を保持するコンポーネント
/// 形状の種類（AABB, Sphere, Capsule）やそのパラメータ、衝突レイヤー等を管理します。
class ColliderComponent : public IComponent {
public:
  /// @brief コライダーの形状定義
  enum class Shape {
    AABB,    ///< 軸平行境界ボックス
    Sphere,  ///< 球体
    Capsule, ///< カプセル
  };

  Shape shape = Shape::AABB; ///< 現在のコライダー形状

  /// @brief AABBパラメータ
  AABB aabb;

  /// @brief Sphereパラメータ
  RC::Vector3 center = {0.0f, 0.0f, 0.0f}; ///< 中心座標（ローカル）
  float radius = 1.0f;                      ///< 半径

  /// @brief 衝突レイヤー（ビットマスク）
  /// どのレイヤーと衝突するかを判定するために使用します。
  uint32_t layer = 0xFFFFFFFF;

  /// @brief トリガーモード設定
  /// true の場合、物理的な衝突応答（押し出し等）を行わず、接触イベントの通知のみを行います。
  bool isTrigger = false;
};
