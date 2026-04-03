#pragma once

#include <d3d12.h>
#include <memory>
#include <vector>

#include "../../Common/struct.h"

class Skydome;
class TextureManager;

namespace RC {

class SkydomeManager {
public:
  void Init(ID3D12Device *device, TextureManager *texman);
  ~SkydomeManager();
  void Term();

  int Create(int textureHandle, float radius = 100.0f, unsigned int sliceCount = 32,
             unsigned int stackCount = 32);
  void Unload(int handle);

  bool IsValid(int handle) const;

  Skydome *Get(int handle);
  const Skydome *Get(int handle) const;

  Transform *GetTransformPtr(int handle);
  void SetColor(int handle, const Vector4 &color);

  void ApplyTexture(int handle, int overrideTexHandle);

private:
  struct Slot {
    std::unique_ptr<Skydome> ptr;
    bool inUse = false;
    int defaultTexHandle = -1;
  };

  int AllocSlot_();

private:
  ID3D12Device *device_ = nullptr;
  TextureManager *texman_ = nullptr;
  std::vector<Slot> skydomes_;
};

} // namespace RC
