#pragma once
#include "RC.h"
#include <d3d12.h>
#include <type_traits>
#include <wrl.h>

namespace RC {

struct ConstBufferDataWorldTransform {
  Matrix4x4 matWorld;
};

class WorldTransform {
public:
  void Initialize();
  void UpdateMatrix();
  void CreateConstBuffer();
  void Map();
  void TransferMatrix();

  const Microsoft::WRL::ComPtr<ID3D12Resource> &GetConstBuffer() const {
    return constBuffer_;
  }

  Vector3 GetWorldPosition() const {
    return {matWorld_.m[3][0], matWorld_.m[3][1], matWorld_.m[3][2]};
  }

  Vector3 GetScale() const { return scale_; }
  void SetScale(const Vector3 &s) { scale_ = s; }
  Vector3 GetRotation() const { return rotation_; }
  void SetRotation(const Vector3 &r) { rotation_ = r; }
  Vector3 GetTranslation() const { return translation_; }
  void SetTranslation(const Vector3 &t) { translation_ = t; }

  const Matrix4x4 &GetMatWorld() const { return matWorld_; }
  void SetParent(const WorldTransform *parent) { parent_ = parent; }

private:
  Vector3 scale_ = {1, 1, 1};
  Vector3 rotation_ = {0, 0, 0};
  Vector3 translation_ = {0, 0, 0};

  Matrix4x4 matWorld_{};
  const WorldTransform *parent_ = nullptr;

  Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
  ConstBufferDataWorldTransform *constMap = nullptr;

  WorldTransform(const WorldTransform &) = delete;
  WorldTransform &operator=(const WorldTransform &) = delete;
};

static_assert(!std::is_copy_assignable_v<WorldTransform>);

} // namespace RC
