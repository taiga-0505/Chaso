#pragma once
#include <cstdint>
#include <d3d12.h>
#include <vector>

class DescriptorHeap;

// シンプルな StructuredBuffer 管理クラス
// - Upload ヒープ上にバッファを確保
// - SRV を共通 CBV/SRV/UAV ヒープ上に作成
// - ID (int) でバッファと SRV を参照
class StructuredBufferManager {
public:
  using BufferID = int;

  void Init(ID3D12Device *device, DescriptorHeap *srvHeap);
  void Term();

  // 要素数と 1 要素あたりのサイズ（バイト）を指定してバッファを作成
  BufferID Create(UINT elementCount, UINT strideBytes);

  // CPU から書き込むために Map / Unmap
  void *Map(BufferID id);
  void Unmap(BufferID id);

  // GPU から参照するための SRV (DescriptorTable 用)
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(BufferID id) const;

  // 生のリソースへのアクセスが必要な場合用
  ID3D12Resource *GetResource(BufferID id) const;

  // 明示的に 1 つだけ破棄したい場合
  void Destroy(BufferID id);

private:
  struct Entry {
    ID3D12Resource *resource = nullptr;
    void *mapped = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE srv{};
    UINT stride = 0;
    UINT count = 0;
    UINT srvIndex = 0;
  };

  ID3D12Device *device_ = nullptr;    // 非所有
  DescriptorHeap *srvHeap_ = nullptr; // 非所有（CBV/SRV/UAV ヒープ）
  std::vector<Entry> entries_;
};
