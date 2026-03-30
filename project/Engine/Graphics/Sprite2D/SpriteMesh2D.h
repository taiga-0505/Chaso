#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

#include "Math/Math.h"
#include "function/function.h"

// Sprite が使う共通頂点フォーマット
struct SpriteVertex {
  RC::Vector4 position; // 0..1 の単位クアッド
  RC::Vector2 texcoord; // 0..1
};

// Sprite の共通部（ModelMesh 相当）
// - 全スプライト共通の「四角形メッシュ(VB/IB)」だけ持つ
class SpriteMesh2D {
public:
  SpriteMesh2D() = default;
  ~SpriteMesh2D();

  bool Initialize(ID3D12Device *device);
  bool Ready() const { return vb_ && ib_; }

  const D3D12_VERTEX_BUFFER_VIEW &VBV() const { return vbv_; }
  const D3D12_INDEX_BUFFER_VIEW &IBV() const { return ibv_; }
  uint32_t IndexCount() const { return 6; }

private:
  void Release_();

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
  Microsoft::WRL::ComPtr<ID3D12Resource> ib_;

  D3D12_VERTEX_BUFFER_VIEW vbv_{};
  D3D12_INDEX_BUFFER_VIEW ibv_{};
};
