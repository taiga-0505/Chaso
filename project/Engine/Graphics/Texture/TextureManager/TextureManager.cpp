#include "TextureManager.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include "SRVManager/SRVManager.h"
#include "Common/Log/Log.h"

void TextureManager::Init(SRVManager *srv) {
  srv_ = srv;
  // テクスチャ転送用に独自のコマンドリスト（DirectQueue、バッファ数1）を作る
  loadCmd_.Init(srv->Device(), D3D12_COMMAND_LIST_TYPE_DIRECT, 1);
  
  // Init() 終了直後は Close されているので、記録開始のために一度 Reset しておく
  loadCmd_.List()->Reset(loadCmd_.GetAllocator(0), nullptr);
}

void TextureManager::Term() {

  // 残っている転送を全て待機
  if (srv_) {
    loadCmd_.FlushGPU();
  }
  ReleasePendingUploads();

  for (auto &[_, tex] : cache_) {
    tex.Term(srv_); // ★必ず srv を渡す
  }
  cache_.clear();
  srv_ = nullptr;
  loadCmd_.Term();
}

void TextureManager::ReleasePendingUploads() {
  uint64_t completed = loadCmd_.GetCompletedFenceValue();
  auto it = pendingUploads_.begin();
  while (it != pendingUploads_.end()) {
    if (it->fenceValue <= completed) {
      it = pendingUploads_.erase(it);
    } else {
      ++it;
    }
  }
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::Load(const std::string &path,
                                                 bool srgb) {
  assert(srv_);
  ReleasePendingUploads(); // 前の転送が終わっていれば解放

  auto it = cache_.find(path);
  if (it != cache_.end() && it->second.IsLoaded()) {
    return it->second.GpuSrv();
  }
  Texture2D &tex = cache_[path];
  auto uploadRes = tex.LoadFromFile(*srv_, loadCmd_, path, srgb);
  if (uploadRes) {
    Log::Print("[Texture] Loaded: " + path);
    uint64_t fence = loadCmd_.ExecuteAndReset();
    pendingUploads_.push_back({uploadRes, fence});
  }

  return tex.GpuSrv();

}

Texture2D *TextureManager::Get(const std::string &path) {
  auto it = cache_.find(path);
  if (it == cache_.end())
    return nullptr;
  return &it->second;
}

// TextureManager.cpp
TextureManager::TextureID TextureManager::LoadID(const std::string &path,
                                                 bool srgb) {
  assert(srv_);
  ReleasePendingUploads(); // 前の転送が終わっていれば解放

  // 既にIDがあるなら返す（未ロードならロード）
  if (auto itId = pathToId_.find(path); itId != pathToId_.end()) {
    auto &tex = cache_[path];
    if (!tex.IsLoaded()) {
      auto uploadRes = tex.LoadFromFile(*srv_, loadCmd_, path, srgb);
      if (uploadRes) {
        Log::Print("[Texture] Loaded: " + path);
        uint64_t fence = loadCmd_.ExecuteAndReset();
        pendingUploads_.push_back({uploadRes, fence});
      }
    }
    return itId->second;
  }

  // 新規
  auto &tex = cache_[path];
  if (!tex.IsLoaded()) {
    auto uploadRes = tex.LoadFromFile(*srv_, loadCmd_, path, srgb);
    if (uploadRes) {
      Log::Print("[Texture] Loaded: " + path);
      uint64_t fence = loadCmd_.ExecuteAndReset();
      pendingUploads_.push_back({uploadRes, fence});
    }
  }


  TextureID id = nextId_++;

  pathToId_[path] = id;
  idToPath_[id] = path;
  return id;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrv(TextureID id) const {
  static D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{}; // 失敗時用
  auto it = idToPath_.find(id);
  if (it == idToPath_.end())
    return nullHandle;
  auto itTex = cache_.find(it->second);
  if (itTex == cache_.end() || !itTex->second.IsLoaded())
    return nullHandle;
  return itTex->second.GpuSrv(); // 既存APIで取得
}

const DirectX::TexMetadata *TextureManager::GetMeta(TextureID id) const {
  auto it = idToPath_.find(id);
  if (it == idToPath_.end())
    return nullptr;
  auto itTex = cache_.find(it->second);
  if (itTex == cache_.end() || !itTex->second.IsLoaded())
    return nullptr;
  return &itTex->second.Metadata(); // 既存の取得関数を流用
}

Texture2D *TextureManager::GetTexture(TextureID id) {
  auto it = idToPath_.find(id);
  if (it == idToPath_.end())
    return nullptr;
  return Get(it->second);
}

std::string
TextureManager::GetPathBySrv(D3D12_GPU_DESCRIPTOR_HANDLE srv) const {
  if (srv.ptr == 0)
    return {};

  for (const auto &kv : cache_) {
    const auto &path = kv.first;
    const auto &tex = kv.second;
    if (tex.IsLoaded() && tex.GpuSrv().ptr == srv.ptr) {
      return path;
    }
  }
  return {};
}
