#pragma once
#include "Texture/Texture2D/Texture2D.h"
#include "Dx12/CommandContext/CommandContext.h"
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <future>
#include <wrl/client.h>

class DescriptorHeap;

class TextureManager {
public:
  using TextureID = int;

  struct UploadTask {
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadRes;
    uint64_t fenceValue;
  };

  void Init(SRVManager *srv);

  void Term();

  // 同じパスはキャッシュして再利用
  // 戻り値：GPU の SRV ハンドル（そのまま SetGraphicsRootDescriptorTable
  // に渡せる）
  D3D12_GPU_DESCRIPTOR_HANDLE Load(const std::string &path, bool srgb = true);

  // 直接 Texture2D にアクセスしたい場合
  Texture2D *Get(const std::string &path);

  TextureID LoadID(const std::string &path, bool srgb = true);
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(TextureID id) const;
  const DirectX::TexMetadata *GetMeta(TextureID id) const;
  Texture2D *GetTexture(TextureID id);

  // SRV(GPUハンドル)からロード元パスを逆引き（見つからなければ空文字）
  std::string GetPathBySrv(D3D12_GPU_DESCRIPTOR_HANDLE srv) const;

  /// <summary>
  /// このシーンでのログ出力履歴をクリアする
  /// </summary>
  void ClearLogHistory() { loggedPaths_.clear(); }

private:
  SRVManager *srv_ = nullptr;
  CommandContext loadCmd_;
  std::vector<UploadTask> pendingUploads_;
  std::unordered_map<std::string, Texture2D> cache_;

  void ReleasePendingUploads();

  int nextId_ = 1;
  std::unordered_map<std::string, TextureID> pathToId_;
  std::unordered_map<TextureID, std::string> idToPath_;

  std::unordered_set<std::string> loggedPaths_;

  mutable std::recursive_mutex mtx_;
};
