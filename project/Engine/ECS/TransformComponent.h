#pragma once

#include "IComponent.h"
#include "Math/MathTypes.h"
#include "Math/Math.h"

/// @brief 位置・回転・スケールを管理するコンポーネント
/// 既存の Transform 構造体との互換性を持ち、親子関係による行列計算をサポートします。
class TransformComponent : public IComponent {
public:
  RC::Vector3 position = {0.0f, 0.0f, 0.0f}; ///< 位置 (x, y, z)
  RC::Vector3 rotation = {0.0f, 0.0f, 0.0f}; ///< 回転 (Euler角: x, y, z)
  RC::Vector3 scale = {1.0f, 1.0f, 1.0f};    ///< スケール (x, y, z)

  /// @brief ワールド行列を計算して取得する
  /// @return 親のトランスフォームを考慮した最終的なワールド行列
  RC::Matrix4x4 GetWorldMatrix() const {
    RC::Matrix4x4 local = MakeAffineMatrix(scale, rotation, position);
    if (parent_) {
      return Multiply(local, parent_->GetWorldMatrix());
    }
    return local;
  }

  /// @brief 既存の Transform 構造体から値を設定する
  /// @param t 元となる Transform 構造体
  void SetFromTransform(const Transform &t) {
    scale = t.scale;
    rotation = t.rotation;
    position = t.translation;
  }

  /// @brief 現在の値を Transform 構造体として取得する
  /// @return 現在のパラメータを持つ Transform 構造体
  Transform ToTransform() const {
    Transform t;
    t.scale = scale;
    t.rotation = rotation;
    t.translation = position;
    return t;
  }

  /// @brief 親となるトランスフォームを設定する
  /// @param parent 親コンポーネントへのポインタ
  void SetParent(TransformComponent *parent) { parent_ = parent; }
  
  /// @brief 親トランスフォームを取得する
  /// @return 親コンポーネントへのポインタ。なければ nullptr
  TransformComponent *GetParent() const { return parent_; }

private:
  TransformComponent *parent_ = nullptr; ///< 親へのポインタ（非所有）
};
