#include "Light/Area/AreaLightManager.h"
#include "Dx12/Dx12Core.h" // CreateBufferResource
#include <algorithm>
#include "function/function.h"

namespace RC {

static UINT Align256_(UINT size) { return (size + 255u) & ~255u; }

void AreaLightManager::Init(ID3D12Device *device) {
  Term();
  device_ = device;
  initialized_ = (device_ != nullptr);
  slots_.clear();
  slots_.shrink_to_fit();

  activeCount_ = 0;
  active_.fill(-1);

  if (!initialized_)
    return;

  // slot[0] = デフォルト（基本OFF）
  slots_.emplace_back();
  slots_[0].inUse = true;

  auto &d = slots_[0].light.Data();
  d.intensity = 0.0f;
  d.range = 0.0f;

  EnsureCB_();
  SyncCB_();
}

void AreaLightManager::Term() {
  slots_.clear();

  activeCount_ = 0;
  active_.fill(-1);

  if (cb_) {
    if (mapped_) {
      cb_->Unmap(0, nullptr);
      mapped_ = nullptr;
    }
    cb_->Release();
    cb_ = nullptr;
  }

  device_ = nullptr;
  initialized_ = false;
}

int AreaLightManager::AllocSlot_() {
  for (int i = 1; i < (int)slots_.size(); ++i) {
    if (!slots_[i].inUse)
      return i;
  }
  slots_.emplace_back();
  return (int)slots_.size() - 1;
}

bool AreaLightManager::IsValid_(int handle) const {
  return (initialized_ && handle >= 0 && handle < (int)slots_.size() &&
          slots_[handle].inUse);
}

int AreaLightManager::ResolveFallbackHandle_() const {
  return 0; // activeが無い時は default(0)
}

int AreaLightManager::Create() {
  if (!initialized_ || !device_)
    return -1;

  const int h = AllocSlot_();
  auto &slot = slots_[h];
  slot.inUse = true;
  slot.light = AreaLightSource{}; // defaults

  // 最初の1個は自動でアクティブに（好みで消してOK）
  if (activeCount_ == 0) {
    AddActive(h);
  }

  return h;
}

void AreaLightManager::Destroy(int handle) {
  if (!IsValid_(handle))
    return;
  if (handle == 0)
    return;

  RemoveActive(handle);
  slots_[handle].inUse = false;
}

void AreaLightManager::SetActive(int handle) {
  ClearActive();
  if (handle >= 0) {
    AddActive(handle);
  }
}

void AreaLightManager::ClearActive() {
  activeCount_ = 0;
  active_.fill(-1);
}

bool AreaLightManager::AddActive(int handle) {
  if (!IsValid_(handle))
    return false;

  // 既に入ってるなら OK 扱い
  for (int i = 0; i < activeCount_; ++i) {
    if (active_[i] == handle)
      return true;
  }

  if (activeCount_ >= kMaxActive)
    return false;

  active_[activeCount_] = handle;
  ++activeCount_;
  return true;
}

void AreaLightManager::RemoveActive(int handle) {
  for (int i = 0; i < activeCount_; ++i) {
    if (active_[i] == handle) {
      for (int j = i; j < activeCount_ - 1; ++j) {
        active_[j] = active_[j + 1];
      }
      active_[activeCount_ - 1] = -1;
      --activeCount_;
      return;
    }
  }
}

int AreaLightManager::GetActiveHandleAt(int index) const {
  if (index < 0 || index >= activeCount_)
    return -1;
  return active_[index];
}

AreaLightSource *AreaLightManager::Get(int handle) {
  if (!IsValid_(handle))
    return nullptr;
  return &slots_[handle].light;
}

const AreaLightSource *AreaLightManager::Get(int handle) const {
  if (!IsValid_(handle))
    return nullptr;
  return &slots_[handle].light;
}

AreaLightSource *AreaLightManager::GetActive() {
  if (activeCount_ > 0) {
    return Get(active_[0]);
  }
  return Get(ResolveFallbackHandle_());
}

const AreaLightSource *AreaLightManager::GetActive() const {
  if (activeCount_ > 0) {
    return Get(active_[0]);
  }
  return Get(ResolveFallbackHandle_());
}

void AreaLightManager::EnsureCB_() {
  if (cb_ || !device_)
    return;

  const UINT size = Align256_((UINT)sizeof(::AreaLightsCB));
  cb_ = CreateBufferResource(device_, size);
  cb_->Map(0, nullptr, reinterpret_cast<void **>(&mapped_));
}

void AreaLightManager::SyncCB_() {
  if (!mapped_)
    return;

  ::AreaLightsCB cb{};
  cb.count = (uint32_t)(std::min)(activeCount_, kMaxActive);

  for (uint32_t i = 0; i < cb.count; ++i) {
    const int h = active_[i];
    if (!IsValid_(h))
      continue;
    cb.lights[i] = slots_[h].light.Data();
  }

  *mapped_ = cb;
}

D3D12_GPU_VIRTUAL_ADDRESS AreaLightManager::GetCBAddress() {
  if (!initialized_)
    return 0;
  EnsureCB_();
  SyncCB_();
  return cb_ ? cb_->GetGPUVirtualAddress() : 0;
}

void AreaLightManager::DrawImGui(int handle, const char *name) {
  if (!IsValid_(handle))
    return;
  slots_[handle].light.DrawImGui(name);
}

} // namespace RC
