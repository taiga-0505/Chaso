#pragma once
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <vector>

#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode
#include "Math/Math.h"
#include "function/function.h" // CreateBufferResource

class Primitive3D {
public:
  Primitive3D() = default;
  ~Primitive3D();

  void Initialize(ID3D12Device *device);
  void Term();

  // 3Dパス開始（PreDraw3D から呼ぶ想定）
  void BeginFrame(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj,
                  BlendMode blendAt3D = kBlendModeNone);

  void AddLine(const RC::Vector3 &a, const RC::Vector3 &b,
               const RC::Vector4 &color, bool depth = true);

  void AddAABB(const RC::Vector3 &mn, const RC::Vector3 &mx,
               const RC::Vector4 &color, bool depth = true);

  void AddGridXZ(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  void AddGridXY(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  void AddGridYZ(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  // RenderCommon 側で PSO/Root はセット済み前提
  void Draw(ID3D12GraphicsCommandList *cl, bool depth);

  bool HasAny() const { return !vtxDepth_.empty() || !vtxNoDepth_.empty(); }
  BlendMode BlendAt3D() const { return blendAt3D_; }

  void Clear();

private:
  struct Vertex {
    RC::Vector3 pos;
    RC::Vector4 color;
  };

  struct alignas(16) PerFrameCB {
    RC::Matrix4x4 View;
    RC::Matrix4x4 Proj;
  };

  static constexpr uint32_t kMaxDrawPerFrame = 64;
  static constexpr uint32_t Align256(uint32_t v) { return (v + 255u) & ~255u; }

  void EnsureVB_(size_t vertexCount);

private:
  ID3D12Device *device_ = nullptr;

  // vertices
  std::vector<Vertex> vtxDepth_;
  std::vector<Vertex> vtxNoDepth_;

  // view/proj for this frame
  PerFrameCB perFrame_{};
  BlendMode blendAt3D_ = kBlendModeNone;

  // VB
  ID3D12Resource *vb_ = nullptr;
  Vertex *vbMap_ = nullptr;
  size_t vbCapacity_ = 0; // vertex count
  D3D12_VERTEX_BUFFER_VIEW vbView_{};

  // CB ring
  ID3D12Resource *cbRes_ = nullptr;
  uint8_t *cbMap_ = nullptr;
  uint32_t cbStride_ = 0;
  uint32_t cbCursor_ = 0;
};
