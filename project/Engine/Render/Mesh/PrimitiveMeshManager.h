#pragma once
#include <d3d12.h>
#include <memory>
#include <vector>
#include "struct.h"

class PrimitiveMesh;
class TextureManager;

namespace RC {

class PrimitiveMeshManager {
public:
  void Init(ID3D12Device *device, TextureManager *texman);
  ~PrimitiveMeshManager();
  void Term();

  int Create(const ModelData &data, int textureHandle = -1);
  void Unload(int handle);

  bool IsValid(int handle) const;

  PrimitiveMesh *Get(int handle);
  const PrimitiveMesh *Get(int handle) const;

  Transform *GetTransformPtr(int handle);
  void SetColor(int handle, const Vector4 &color);

  void ApplyTexture(int handle, int overrideTexHandle);
  void DrawImGui(int handle, const char *name);

private:
  struct Slot {
    std::unique_ptr<PrimitiveMesh> ptr;
    bool inUse = false;
    int defaultTexHandle = -1;
  };

  int AllocSlot_();

private:
  ID3D12Device *device_ = nullptr;
  TextureManager *texman_ = nullptr;
  D3D12_GPU_DESCRIPTOR_HANDLE whiteSrv_{};
  std::vector<Slot> primitives_;
};

} // namespace RC
