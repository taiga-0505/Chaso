#include "StructuredBufferManager.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include <cassert>

void StructuredBufferManager::Init(ID3D12Device *device,
                                   DescriptorHeap *srvHeap) {
  assert(device);
  assert(srvHeap);
  device_ = device;
  srvHeap_ = srvHeap;
  entries_.clear();
}

void StructuredBufferManager::Term() {
  for (auto &e : entries_) {
    if (e.resource) {
      if (e.mapped) {
        e.resource->Unmap(0, nullptr);
        e.mapped = nullptr;
      }
      e.resource->Release();
      e.resource = nullptr;
    }
    e.srv = {};
    e.stride = 0;
    e.count = 0;
  }
  entries_.clear();
  device_ = nullptr;
  srvHeap_ = nullptr;
}

StructuredBufferManager::BufferID
StructuredBufferManager::Create(UINT elementCount, UINT strideBytes) {
  assert(device_);
  assert(srvHeap_);
  assert(elementCount > 0);
  assert(strideBytes > 0);

  // ---- 1) Upload ヒープ上にバッファ作成 ----
  const UINT64 totalSize = static_cast<UINT64>(elementCount) * strideBytes;

  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_DESC resDesc{};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = totalSize;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.Format = DXGI_FORMAT_UNKNOWN;
  resDesc.SampleDesc.Count = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  ID3D12Resource *resource = nullptr;
  HRESULT hr = device_->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr) && "Failed to create structured buffer resource");

  // ---- 2) SRV 作成 ----
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvHeap_->AllocateCPU();

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = DXGI_FORMAT_UNKNOWN;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Buffer.FirstElement = 0;
  srvDesc.Buffer.NumElements = elementCount;
  srvDesc.Buffer.StructureByteStride = strideBytes;
  srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

  device_->CreateShaderResourceView(resource, &srvDesc, cpuHandle);

  // AllocateCPU の直後は Used()-1 が今作った SRV の index
  const UINT srvIndex = srvHeap_->Used() - 1;
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = srvHeap_->GPUAt(srvIndex);

  // ---- 3) 登録 ----
  Entry entry{};
  entry.resource = resource;
  entry.mapped = nullptr;
  entry.srv = gpuHandle;
  entry.stride = strideBytes;
  entry.count = elementCount;
  entry.srvIndex = srvIndex;

  entries_.push_back(entry);
  return static_cast<BufferID>(entries_.size() - 1);
}

void *StructuredBufferManager::Map(BufferID id) {
  if (id < 0 || static_cast<size_t>(id) >= entries_.size()) {
    return nullptr;
  }
  Entry &e = entries_[id];
  if (!e.resource) {
    return nullptr;
  }
  if (!e.mapped) {
    HRESULT hr = e.resource->Map(0, nullptr, &e.mapped);
    assert(SUCCEEDED(hr) && "Failed to map structured buffer");
  }
  return e.mapped;
}

void StructuredBufferManager::Unmap(BufferID id) {
  if (id < 0 || static_cast<size_t>(id) >= entries_.size()) {
    return;
  }
  Entry &e = entries_[id];
  if (e.resource && e.mapped) {
    e.resource->Unmap(0, nullptr);
    e.mapped = nullptr;
  }
}

D3D12_GPU_DESCRIPTOR_HANDLE
StructuredBufferManager::GetSrv(BufferID id) const {
  if (id < 0 || static_cast<size_t>(id) >= entries_.size()) {
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }
  return entries_[id].srv;
}

ID3D12Resource *StructuredBufferManager::GetResource(BufferID id) const {
  if (id < 0 || static_cast<size_t>(id) >= entries_.size()) {
    return nullptr;
  }
  return entries_[id].resource;
}

void StructuredBufferManager::Destroy(BufferID id) {
  if (id < 0 || static_cast<size_t>(id) >= entries_.size()) {
    return;
  }
  Entry &e = entries_[id];
  if (e.resource) {
    if (e.mapped) {
      e.resource->Unmap(0, nullptr);
      e.mapped = nullptr;
    }
    e.resource->Release();
    e.resource = nullptr;
  }
  e.srv = {};
  e.stride = 0;
  e.count = 0;
  e.srvIndex = 0;
}
