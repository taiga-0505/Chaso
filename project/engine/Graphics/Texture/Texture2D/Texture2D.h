#pragma once
#include "DirectXTex/DirectXTex.h"
#include "SRVManager/SRVManager.h"
#include <cassert>
#include <d3d12.h>
#include <string>
#include <wrl/client.h>

class SRVManager;
class CommandContext;

class Texture2D {
public:
  Texture2D() = default;
  ~Texture2D() { Term(); }

  Microsoft::WRL::ComPtr<ID3D12Resource> LoadFromFile(SRVManager &srv, CommandContext &cmd, const std::string &path, bool srgb = true);
  void Term(SRVManager *srv = nullptr);


  Texture2D(const Texture2D &) = delete;
  Texture2D &operator=(const Texture2D &) = delete;

  Texture2D(Texture2D &&o) noexcept { *this = std::move(o); }
  Texture2D &operator=(Texture2D &&o) noexcept {
    if (this == &o)
      return *this;

#if _DEBUG
    // ここが非空でムーブ代入されるのは運用ミス
    assert(resource_ == nullptr);
    assert(!srv_.IsValid());
#endif
    path_ = std::move(o.path_);
    resource_ = o.resource_;
    o.resource_ = nullptr;
    srv_ = o.srv_;
    o.srv_ = {};
    mipImages_ = std::move(o.mipImages_);
    metadata_ = o.metadata_;
    o.metadata_ = {};
    return *this;
  }
  // 取得系
  ID3D12Resource *Resource() const { return resource_.Get(); }
  D3D12_GPU_DESCRIPTOR_HANDLE GpuSrv() const { return srv_.gpu; }
  D3D12_CPU_DESCRIPTOR_HANDLE CpuSrv() const { return srv_.cpu; }
  const DirectX::TexMetadata &Metadata() const { return metadata_; }
  const std::string &Path() const { return path_; }
  bool IsLoaded() const { return resource_ != nullptr; }

private:
  std::string path_;
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
  SRVManager::Handle srv_{};
  DirectX::ScratchImage mipImages_;
  DirectX::TexMetadata metadata_{};
};
