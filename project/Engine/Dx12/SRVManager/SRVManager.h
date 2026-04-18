#pragma once
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <vector>

class DescriptorHeap;

class SRVManager {
public:
  struct Handle {
    UINT index = UINT_MAX;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    bool IsValid() const { return index != UINT_MAX; }
  };

  void Init(ID3D12Device *device, DescriptorHeap *srvHeap);
  void Term();

  ID3D12Device *Device() const { return device_; }
  DescriptorHeap *Heap() const { return srvHeap_; }

  // SRVスロットを確保（生のスロットだけ欲しい時）
  Handle Allocate();

  // 使い終わったスロットを戻す（※GPUが参照し終わってから呼ぶのが前提）
  void Free(const Handle &h);

  // よく使うSRV作成をここに集約
  Handle CreateTexture2D(ID3D12Resource *res, DXGI_FORMAT fmt, UINT mipLevels,
                         UINT mostDetailedMip = 0);

  Handle CreateTextureCube(ID3D12Resource *res, DXGI_FORMAT fmt, UINT mipLevels,
                           UINT mostDetailedMip = 0);

  Handle CreateStructuredBuffer(ID3D12Resource *res, UINT elementCount,
                                UINT strideBytes);

private:
  ID3D12Device *device_ = nullptr; // 非所有
  DescriptorHeap *srvHeap_ =
      nullptr; // 非所有（CBV/SRV/UAVのshader visible heap）

  std::vector<UINT> free_;    // 再利用用の空きindex
  std::vector<uint8_t> used_; // 0=free, 1=used（デバッグ用）
};
