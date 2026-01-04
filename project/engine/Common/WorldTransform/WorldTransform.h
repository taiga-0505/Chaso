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
  Vector3 scale = {1, 1, 1};
  Vector3 rotation = {0, 0, 0};
  Vector3 translation = {0, 0, 0};

  Matrix4x4 matWorld{};
  const WorldTransform *parent_ = nullptr;

  void Initialize();
  void UpdateMatrix();
  void CreateConstBuffer();
  void Map();
  void TransferMatrix();

  const Microsoft::WRL::ComPtr<ID3D12Resource> &GetConstBuffer() const {
    return constBuffer_;
  }

  Vector3 GetWorldPosition() const {
    return {matWorld.m[3][0], matWorld.m[3][1], matWorld.m[3][2]};
  }

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
  ConstBufferDataWorldTransform *constMap = nullptr;

  WorldTransform(const WorldTransform &) = delete;
  WorldTransform &operator=(const WorldTransform &) = delete;
};

static_assert(!std::is_copy_assignable_v<WorldTransform>);

} // namespace RC
