#pragma once

// ============================================================================
// TransformComponent
// ----------------------------------------------------------------------------
// 位置・回転・スケールを持つコンポーネント。
// 既存の Transform 構造体と互換性を持ち、親子関係もサポートする。
// ============================================================================

#include "IComponent.h"
#include "Math/MathTypes.h"
#include "Math/Math.h"

class TransformComponent : public IComponent {
public:
  RC::Vector3 position = {0.0f, 0.0f, 0.0f};
  RC::Vector3 rotation = {0.0f, 0.0f, 0.0f};
  RC::Vector3 scale = {1.0f, 1.0f, 1.0f};

  /// ワールド行列を計算して返す
  RC::Matrix4x4 GetWorldMatrix() const {
    RC::Matrix4x4 local = MakeAffineMatrix(scale, rotation, position);
    if (parent_) {
      return Multiply(local, parent_->GetWorldMatrix());
    }
    return local;
  }

  /// 既存の Transform 構造体から設定
  void SetFromTransform(const Transform &t) {
    scale = t.scale;
    rotation = t.rotation;
    position = t.translation;
  }

  /// 既存の Transform 構造体へ変換
  Transform ToTransform() const {
    Transform t;
    t.scale = scale;
    t.rotation = rotation;
    t.translation = position;
    return t;
  }

  /// 親子関係の設定
  void SetParent(TransformComponent *parent) { parent_ = parent; }
  TransformComponent *GetParent() const { return parent_; }

private:
  TransformComponent *parent_ = nullptr;
};
