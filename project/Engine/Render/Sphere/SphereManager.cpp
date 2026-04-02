#include "SphereManager.h"

#include "Sphere/Sphere.h"
#include "Texture/TextureManager/TextureManager.h"

namespace RC {

SphereManager::~SphereManager() = default;

void SphereManager::Init(ID3D12Device *device, TextureManager *texman) {
  device_ = device;
  texman_ = texman;
  spheres_.clear();
}

void SphereManager::Term() {
  spheres_.clear();
  device_ = nullptr;
  texman_ = nullptr;
}

int SphereManager::AllocSlot_() {
  for (int i = 0; i < static_cast<int>(spheres_.size()); ++i) {
    if (!spheres_[i].inUse) {
      return i;
    }
  }
  spheres_.emplace_back();
  return static_cast<int>(spheres_.size()) - 1;
}

int SphereManager::Create(int textureHandle, float radius, unsigned int sliceCount,
                          unsigned int stackCount, bool inward) {
  if (!device_) {
    return -1;
  }

  const int handle = AllocSlot_();

  auto sphere = std::make_unique<Sphere>();
  sphere->Initialize(device_, radius, sliceCount, stackCount, inward);

  if (textureHandle >= 0 && texman_) {
    sphere->SetTexture(texman_->GetSrv(textureHandle));
  }

  spheres_[handle].ptr = std::move(sphere);
  spheres_[handle].inUse = true;
  spheres_[handle].defaultTexHandle = textureHandle;

  return handle;
}

void SphereManager::Unload(int handle) {
  if (!IsValid(handle)) {
    return;
  }

  spheres_[handle].ptr.reset();
  spheres_[handle].inUse = false;
  spheres_[handle].defaultTexHandle = -1;
}

bool SphereManager::IsValid(int handle) const {
  return (handle >= 0 && handle < static_cast<int>(spheres_.size()) &&
          spheres_[handle].inUse && spheres_[handle].ptr);
}

Sphere *SphereManager::Get(int handle) {
  if (!IsValid(handle)) {
    return nullptr;
  }
  return spheres_[handle].ptr.get();
}

const Sphere *SphereManager::Get(int handle) const {
  if (!IsValid(handle)) {
    return nullptr;
  }
  return spheres_[handle].ptr.get();
}

Transform *SphereManager::GetTransformPtr(int handle) {
  if (!IsValid(handle)) {
    return nullptr;
  }
  return &spheres_[handle].ptr->T();
}

void SphereManager::SetColor(int handle, const Vector4 &color) {
  if (!IsValid(handle)) {
    return;
  }

  if (auto *mat = spheres_[handle].ptr->Mat()) {
    mat->color = color;
  }
}

void SphereManager::SetLightingMode(int handle, LightingMode m) {
  if (!IsValid(handle)) {
    return;
  }

  if (auto *mat = spheres_[handle].ptr->Mat()) {
    mat->lightingMode = static_cast<int>(m);
  }
}

void SphereManager::ApplyTexture(int handle, int overrideTexHandle) {
  if (!IsValid(handle) || !texman_) {
    return;
  }

  const int useTex = (overrideTexHandle >= 0)
                         ? overrideTexHandle
                         : spheres_[handle].defaultTexHandle;
  if (useTex >= 0) {
    spheres_[handle].ptr->SetTexture(texman_->GetSrv(useTex));
  }
}

} // namespace RC
