#include "Texture2D.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include "DescriptorHeap/DescriptorHelpers.h"
#include "function/function.h"

void Texture2D::LoadFromFile(SRVManager &srv, const std::string &path,
                             bool srgb) {
  Term(&srv);
  ID3D12Device *device = srv.Device();

  // ---- 1) 画像読み込み（存在しなければ白1x1）----
  std::wstring wpath(path.begin(), path.end());
  DirectX::ScratchImage image;

  const DirectX::WIC_FLAGS wicFlags =
      srgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB;

  HRESULT hr =
      DirectX::LoadFromWICFile(wpath.c_str(), wicFlags, nullptr, image);

  if (FAILED(hr)) {
    // 白1x1を生成（ここはUNORMで作って、後でsrgbならMakeSRGBする）
    DirectX::ScratchImage white;
    white.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
    auto img = white.GetImage(0, 0, 0);
    uint8_t *p = img->pixels;
    p[0] = 255;
    p[1] = 255;
    p[2] = 255;
    p[3] = 255;
    mipImages_ = std::move(white);
  } else {
    // ミップ生成（srgbならsRGBフィルタ、linearなら通常フィルタ）
    const DirectX::TEX_FILTER_FLAGS mipFilter =
        srgb ? DirectX::TEX_FILTER_SRGB : DirectX::TEX_FILTER_DEFAULT;

    DirectX::ScratchImage mips;
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                                  image.GetMetadata(), mipFilter, 0, mips);

    mipImages_ = SUCCEEDED(hr) ? std::move(mips) : std::move(image);
  }

  metadata_ = mipImages_.GetMetadata();

  // srgb=trueならSRGB化、srgb=falseなら線形（非SRGB）に戻す
  if (srgb) {
    metadata_.format = DirectX::MakeSRGB(metadata_.format);
  } else {
    metadata_.format = DirectX::MakeLinear(metadata_.format);
  }

  // ---- 2) GPUテクスチャ作成（WriteBack方式：今の実装方針のまま）----
  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION(metadata_.dimension);
  desc.Width = (UINT)metadata_.width;
  desc.Height = (UINT)metadata_.height;
  desc.DepthOrArraySize = (UINT16)metadata_.arraySize;
  desc.MipLevels = (UINT16)metadata_.mipLevels;
  desc.Format = metadata_.format;
  desc.SampleDesc.Count = 1;

  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_CUSTOM;
  heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
  heap.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

  hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                       nullptr, IID_PPV_ARGS(&resource_));
  assert(SUCCEEDED(hr));

  // ---- 3) サブリソースに書き込み ----
  for (size_t mip = 0; mip < metadata_.mipLevels; ++mip) {
    const DirectX::Image *img = mipImages_.GetImage(mip, 0, 0);
    hr = resource_->WriteToSubresource((UINT)mip, nullptr, img->pixels,
                                       (UINT)img->rowPitch,
                                       (UINT)img->slicePitch);
    assert(SUCCEEDED(hr));
  }

  // ---- SRV作成 ----
  srv_ = srv.CreateTexture2D(resource_, metadata_.format,
                             (UINT)metadata_.mipLevels);
  path_ = path;
}

void Texture2D::Term(SRVManager *srv) {
#if _DEBUG
  if (!srv && srv_.IsValid()) {
    assert(false && "Texture2D SRV is still allocated. Call Term(&srvMgr) "
                    "before destruction.");
  }
#endif
  if (resource_) {
    resource_->Release();
    resource_ = nullptr;
  }
  if (srv && srv_.IsValid()) {
    // ※GPUが参照し終わったタイミングで呼ぶのが前提（終了時ならOK）
    srv->Free(srv_);
  }
  srv_ = {};
  path_.clear();
  mipImages_ = DirectX::ScratchImage{};
  metadata_ = DirectX::TexMetadata{};
}
