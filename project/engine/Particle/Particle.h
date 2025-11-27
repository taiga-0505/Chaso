#pragma once
#include "Scene.h"
#include "struct.h"
#include <d3d12.h>
#include <random>
#include <wrl/client.h>

class StructuredBufferManager;

namespace RC {

class Particle {
public:
  void Initialize(SceneContext &ctx);
  void Finalize();
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  void DrawImGui();

  ParticleData MakeNewParticle(std::mt19937 &randomEngine);

private:
private:
  ModelData modelData;
  static const int instanceCount = 10;

  ParticleData particles[instanceCount];

  ID3D12Device *device_ = nullptr;

  // ▼ StructuredBufferManager 管理のバッファ
  ::StructuredBufferManager *sbMgr_ = nullptr; // 非所有
  int instanceBufferId_ = -1;
  ParticleForGPU *instancingData_ = nullptr;                           // Map先
  D3D12_GPU_DESCRIPTOR_HANDLE instanceSrv_{};                          // t0, VS
  Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = nullptr; // 所有

  // 頂点バッファ
  ID3D12Resource *vbResource_ = nullptr;
  D3D12_VERTEX_BUFFER_VIEW vbView_{};
  uint32_t vertexCount_ = 0;

  // Material / Texture
  ID3D12Resource *cbMat_ = nullptr;
  SpriteMaterial *cbMatMapped_ = nullptr;

  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{}; // Particle テクスチャ

  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }

  // 表示／Update 制御
  bool visible_ = true;
  bool enableUpdate_ = true;

  // UV トランスフォーム用パラメータ
  RC::Vector2 uvScale_{1.0f, 1.0f};
  RC::Vector2 uvTranslate_{0.0f, 0.0f};
  float uvRotate_ = 0.0f;

  // ランダム
  std::random_device seedGenerator;
  std::mt19937 randomEngine{seedGenerator()};
};
} // namespace RC
