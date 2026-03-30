#pragma once
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
#include "SRVManager/SRVManager.h"

class SRVManager;

// シンプルな StructuredBuffer 管理クラス
// - Upload ヒープ上にバッファを確保
// - SRV は SRVManager に作成させる（index管理もSRVManagerに委譲）
// - ID (int) でバッファと SRV を参照
class StructuredBufferManager {
public:
  using BufferID = int;

  void Init(SRVManager *srvMgr);
  void Term();

  BufferID Create(UINT elementCount, UINT strideBytes);

  void *Map(BufferID id);
  void Unmap(BufferID id);

  D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(BufferID id) const;
  ID3D12Resource *GetResource(BufferID id) const;

  void Destroy(BufferID id);

private:
  struct Entry {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    void *mapped = nullptr;

    // SRVManager が管理するスロット
    SRVManager::Handle srvHandle{};

    UINT stride = 0;
    UINT count = 0;
  };

  SRVManager *srvMgr_ = nullptr;   // 非所有
  Microsoft::WRL::ComPtr<ID3D12Device> device_; // 非所有（srvMgr_->Device() のキャッシュ）
  std::vector<Entry> entries_;
};
