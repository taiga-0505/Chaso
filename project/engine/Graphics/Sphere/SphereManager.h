#pragma once

#include <d3d12.h>

#include <memory>
#include <vector>

#include "struct.h"

class Sphere;
class TextureManager;

namespace RC {

class SphereManager {
public:
  void Init(ID3D12Device *device, TextureManager *texman);
  void Term();

  int Create(int textureHandle, float radius, unsigned int sliceCount,
             unsigned int stackCount, bool inward);
  void Unload(int handle);

  bool IsValid(int handle) const;

  Sphere *Get(int handle);
  const Sphere *Get(int handle) const;

  Transform *GetTransformPtr(int handle);
  void SetColor(int handle, const Vector4 &color);
  void SetLightingMode(int handle, LightingMode m);

  void ApplyTexture(int handle, int overrideTexHandle);

private:
  struct Slot {
    std::unique_ptr<Sphere> ptr;
    bool inUse = false;
    int defaultTexHandle = -1;
  };

  int AllocSlot_();

private:
  ID3D12Device *device_ = nullptr;
  TextureManager *texman_ = nullptr;
  std::vector<Slot> spheres_;
};

} // namespace RC
