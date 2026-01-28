#include "ModelManager.h"

#include "Model3d/ModelObject.h"
#include "Model3d/ModelMesh.h"

#include <filesystem>

namespace RC {

namespace {

static std::string NormalizeMeshKey_(const std::string &path) {
  namespace fs = std::filesystem;

  std::error_code ec;
  fs::path p(path);

  // 絶対パス化できるなら絶対パス。失敗したら相対のまま lexically_normal。
  fs::path abs = fs::absolute(p, ec);
  fs::path key = ec ? p.lexically_normal() : abs.lexically_normal();
  return key.string();
}

} // namespace

ModelManager::~ModelManager() = default;

void ModelManager::Init(ID3D12Device *device, TextureManager *texman) {
  device_ = device;
  texman_ = texman;
  models_.clear();
  meshCache_.clear();
}

void ModelManager::Term() {
  models_.clear();
  meshCache_.clear();
  device_ = nullptr;
  texman_ = nullptr;
}

int ModelManager::AllocSlot_() {
  for (int i = 0; i < static_cast<int>(models_.size()); ++i) {
    if (!models_[i].inUse) {
      return i;
    }
  }
  models_.emplace_back();
  return static_cast<int>(models_.size()) - 1;
}

bool ModelManager::IsValid(int handle) const {
  return (handle >= 0 && handle < static_cast<int>(models_.size()) &&
          models_[handle].inUse && models_[handle].ptr);
}

::ModelObject *ModelManager::Get(int handle) {
  if (!IsValid(handle)) {
    return nullptr;
  }
  return models_[handle].ptr.get();
}

const ::ModelObject *ModelManager::Get(int handle) const {
  if (!IsValid(handle)) {
    return nullptr;
  }
  return models_[handle].ptr.get();
}

std::shared_ptr<::ModelMesh> ModelManager::GetOrLoadMesh_(
    const std::string &path) {
  if (!device_) {
    return nullptr;
  }

  const std::string key = NormalizeMeshKey_(path);

  // すでにロード済みなら共有
  if (auto it = meshCache_.find(key); it != meshCache_.end()) {
    if (auto sp = it->second.lock()) {
      return sp;
    }
    // 期限切れ（誰も参照していない）なら掃除して入れ直す
    meshCache_.erase(it);
  }

  // 未ロードならロード
  auto mesh = std::make_shared<::ModelMesh>();
  if (!mesh->LoadObj(device_,path)) {
    return nullptr;
  }

  meshCache_[key] = mesh;
  return mesh;
}

int ModelManager::Load(const std::string &path) {
  if (!device_) {
    return -1;
  }

  const int handle = AllocSlot_();

  auto obj = std::make_unique<::ModelObject>();
  obj->Initialize(device_);
  obj->SetTextureManager(texman_);

  auto mesh = GetOrLoadMesh_(path);
  if (!mesh) {
    models_[handle].inUse = false;
    models_[handle].ptr.reset();
    return -1;
  }
  obj->SetMesh(mesh);

  models_[handle].ptr = std::move(obj);
  models_[handle].inUse = true;
  return handle;
}

void ModelManager::Unload(int handle) {
  if (!IsValid(handle)) {
    return;
  }
  models_[handle].ptr.reset();
  models_[handle].inUse = false;
}

Transform *ModelManager::GetTransformPtr(int handle) {
  if (!IsValid(handle)) {
    return nullptr;
  }
  return &models_[handle].ptr->T();
}

void ModelManager::SetColor(int handle, const Vector4 &color) {
  if (!IsValid(handle)) {
    return;
  }
  models_[handle].ptr->SetColor(color);
}

void ModelManager::SetLightingMode(int handle, LightingMode m) {
  if (!IsValid(handle)) {
    return;
  }
  models_[handle].ptr->SetLightingMode(m);
}

void ModelManager::SetMesh(int handle, const std::string &path) {
  if (!IsValid(handle)) {
    return;
  }

  auto mesh = GetOrLoadMesh_(path);
  if (!mesh) {
    return;
  }

  models_[handle].ptr->SetMesh(mesh);
  // Mesh を変えると mtl のテクスチャ参照も変わる可能性があるので戻す
  models_[handle].ptr->ResetTextureToMtl();
}

void ModelManager::ResetCursor(int handle) {
  if (!IsValid(handle)) {
    return;
  }
  models_[handle].ptr->ResetBatchCursor();
}

void ModelManager::ResetAllBatchCursors() {
  for (auto &s : models_) {
    if (s.inUse && s.ptr) {
      s.ptr->ResetBatchCursor();
    }
  }
}

} // namespace RC
