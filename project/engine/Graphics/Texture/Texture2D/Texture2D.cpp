#include "Texture2D.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include "DescriptorHeap/DescriptorHelpers.h"
#include "function/function.h"
#include "Dx12/CommandContext/CommandContext.h"
#include <vector>

Microsoft::WRL::ComPtr<ID3D12Resource> Texture2D::LoadFromFile(SRVManager &srv, CommandContext &cmd, const std::string &path, bool srgb) {
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

  // ---- 2) GPUテクスチャ作成（VRAM方式へ変更）----
  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION(metadata_.dimension);
  desc.Width = (UINT)metadata_.width;
  desc.Height = (UINT)metadata_.height;
  desc.DepthOrArraySize = (UINT16)metadata_.arraySize;
  desc.MipLevels = (UINT16)metadata_.mipLevels;
  desc.Format = metadata_.format;
  desc.SampleDesc.Count = 1;

  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM

  hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                       D3D12_RESOURCE_STATE_COPY_DEST,
                                       nullptr, IID_PPV_ARGS(&resource_));
  assert(SUCCEEDED(hr));

  // ---- 3) IntermediateResource (UploadHeap) の作成と転送 ----
  UINT numMipLevels = (UINT)metadata_.mipLevels;

  std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(numMipLevels);
  std::vector<UINT> numRows(numMipLevels);
  std::vector<UINT64> rowSizesInBytes(numMipLevels);
  uint64_t uploadBufferSize = 0;

  // GetCopyableFootprints で各ミップのフットプリント（配置位置・サイズ等）とバッファ全体サイズを取得
  device->GetCopyableFootprints(&desc, 0, numMipLevels, 0, layouts.data(), numRows.data(), rowSizesInBytes.data(), &uploadBufferSize);

  D3D12_HEAP_PROPERTIES uploadHeap{};
  uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC bufferDesc{};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Width = uploadBufferSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  Microsoft::WRL::ComPtr<ID3D12Resource> uploadRes;
  hr = device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadRes));
  assert(SUCCEEDED(hr));

  uint8_t* pData = nullptr;
  hr = uploadRes->Map(0, nullptr, reinterpret_cast<void**>(&pData));
  assert(SUCCEEDED(hr));

  for (size_t mip = 0; mip < metadata_.mipLevels; ++mip) {
    const DirectX::Image *img = mipImages_.GetImage(mip, 0, 0);
    if (!img) continue; // 安全装置

    const auto& layout = layouts[mip];
    
    // Uploadバッファにコピー
    uint8_t* destSlice = pData + layout.Offset;
    const uint8_t* srcSlice = img->pixels;
    
    // Copy size per row should be the minimum of what the image has and what D3D expects,
    // although typically D3D row pitch is larger (aligned to 256 bytes).
    UINT bytesPerRow = (UINT)std::min<UINT64>(img->rowPitch, rowSizesInBytes[mip]);

    for (UINT y = 0; y < numRows[mip]; ++y) {
      memcpy(destSlice + y * layout.Footprint.RowPitch, srcSlice + y * img->rowPitch, bytesPerRow);
    }


    assert(resource_.Get() != nullptr && "Destination resource is null.");
    assert(uploadRes.Get() != nullptr && "Source upload resource is null.");
    assert(cmd.List() != nullptr && "Command list is null.");

    // CopyTextureRegion のコマンドを積む
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = resource_.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = (UINT)mip;

    D3D12_TEXTURE_COPY_LOCATION srcLocation{};
    srcLocation.pResource = uploadRes.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = layout;

    cmd.List()->CopyTextureRegion(&dst, 0, 0, 0, &srcLocation, nullptr);

  }

  uploadRes->Unmap(0, nullptr);

  // 転送が終わったら SHADER_RESOURCE ステートに遷移する
  cmd.Transition(resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  // ---- SRV作成 ----
  srv_ = srv.CreateTexture2D(resource_.Get(), metadata_.format,
                             (UINT)metadata_.mipLevels);
  path_ = path;

  return uploadRes;
}

void Texture2D::Term(SRVManager *srv) {
#if _DEBUG
  if (!srv && srv_.IsValid()) {
    assert(false && "Texture2D SRV is still allocated. Call Term(&srvMgr) "
                    "before destruction.");
  }
#endif
  if (resource_) {
    resource_.Reset();
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
