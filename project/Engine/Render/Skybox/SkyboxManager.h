#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>

#include "../../Common/struct.h"

class Skybox;
class TextureManager;

namespace RC {

class SkyboxManager {
public:
  void Init(ID3D12Device *device, TextureManager *texman);
  ~SkyboxManager();
  void Term();

  // ムーブのみ許可（デストラクタが .cpp にあるため）
  SkyboxManager() = default;
  SkyboxManager(SkyboxManager &&) = default;
  SkyboxManager &operator=(SkyboxManager &&) = default;
  SkyboxManager(const SkyboxManager &) = delete;
  SkyboxManager &operator=(const SkyboxManager &) = delete;

  /// <summary>
  /// DDSパスからテクスチャロード＋Skybox生成を一括実行
  /// </summary>
  int Create(const std::string &ddsPath);

  void Unload(int handle);

  bool IsValid(int handle) const;

  Skybox *Get(int handle);
  const Skybox *Get(int handle) const;

  Transform *GetTransformPtr(int handle);
  void SetColor(int handle, const Vector4 &color);

  void ApplyTexture(int handle);

private:
  struct Slot {
    std::unique_ptr<Skybox> ptr;
    bool inUse = false;
    D3D12_GPU_DESCRIPTOR_HANDLE texSrv{};
  };

  int AllocSlot_();

private:
  ID3D12Device *device_ = nullptr;
  TextureManager *texman_ = nullptr;
  std::vector<Slot> skyboxes_;
};

} // namespace RC
