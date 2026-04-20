#include "../FrameResource.h"
#include "Common/Log/Log.h"
#include "function/function.h"
#include <cassert>
#include <format>

namespace RC {

void FrameResource::Init(ID3D12Device *device, uint32_t frameIndex) {
  device_ = device;
  frameIndex_ = frameIndex;

  // CB ヒープ
  cbCapacity_ = kDefaultCBSize;
  cbHeap_ = CreateBufferResource(
      device, cbCapacity_,
      std::format(L"FrameResource[{}]::cbHeap_", frameIndex).c_str());
  cbHeap_->Map(0, nullptr, reinterpret_cast<void **>(&cbMapped_));
  cbOffset_ = 0;

  // SRV ヒープ
  srvCapacity_ = kDefaultSRVSize;
  srvHeap_ = CreateBufferResource(
      device, srvCapacity_,
      std::format(L"FrameResource[{}]::srvHeap_", frameIndex).c_str());
  srvHeap_->Map(0, nullptr, reinterpret_cast<void **>(&srvMapped_));
  srvOffset_ = 0;

  Log::Print(std::format("[FrameResource] Frame[{}] initialized (CB: {}KB, SRV: {}KB)",
                         frameIndex, cbCapacity_ / 1024, srvCapacity_ / 1024));
}

void FrameResource::Reset() {
  cbOffset_ = 0;
  srvOffset_ = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::AllocCB(uint32_t sizeBytes,
                                                  void **outMapped) {
  const uint32_t aligned = Align256(sizeBytes);
  assert(cbOffset_ + aligned <= cbCapacity_ &&
         "FrameResource CB capacity exceeded!");

  const uint64_t offset = cbOffset_;
  cbOffset_ += aligned;

  if (outMapped) {
    *outMapped = cbMapped_ + offset;
  }

  return cbHeap_->GetGPUVirtualAddress() + offset;
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::AllocSRV(uint32_t sizeBytes,
                                                   void **outMapped) {
  assert(srvOffset_ + sizeBytes <= srvCapacity_ &&
         "FrameResource SRV capacity exceeded!");

  const uint64_t offset = srvOffset_;
  srvOffset_ += sizeBytes;

  if (outMapped) {
    *outMapped = srvMapped_ + offset;
  }

  return srvHeap_->GetGPUVirtualAddress() + offset;
}

bool FrameResource::HasCBSpace(uint32_t sizeBytes) const {
  return cbOffset_ + Align256(sizeBytes) <= cbCapacity_;
}

bool FrameResource::HasSRVSpace(uint32_t sizeBytes) const {
  return srvOffset_ + sizeBytes <= srvCapacity_;
}

} // namespace RC
