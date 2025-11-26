#pragma once
#include "struct.h"
#include <d3d12.h>

namespace RC {

class Particle {
public:
  void Initialize(ID3D12Device *device);
  void Finalize();
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);
  void Render(ID3D12GraphicsCommandList *cl);

private:
  ModelData modelData;
  int instanceCount = 10;

  ID3D12Device *device_ = nullptr;

  // 粒の WVP をまとめて入れておく CB
  ID3D12Resource *cbWvp_ = nullptr;
  uint8_t *cbWvpMapped_ = nullptr;
  uint32_t cbStride_ = 0; // Align256(sizeof(TransformationMatrix))

  // 頂点バッファ
  ID3D12Resource *vbResource_ = nullptr;
  D3D12_VERTEX_BUFFER_VIEW vbView_{};
  uint32_t vertexCount_ = 0;

   // Material / Light / Texture
  ID3D12Resource *cbMat_ = nullptr;
  Material *cbMatMapped_ = nullptr;

  ID3D12Resource *cbLight_ = nullptr;
  DirectionalLight *cbLightMapped_ = nullptr;

  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{}; // uvChecker 用

  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }
};

} // namespace RC
