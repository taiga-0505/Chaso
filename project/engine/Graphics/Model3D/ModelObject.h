#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "ModelMesh.h"
#include "function/function.h"
#include "struct.h"
#include <array>
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>

class TextureManager; // 前方宣言

// 配置（個別）：Transform / CB / テクスチャ差し替え / 描画
class ModelObject {
public:
  struct LightingConfig {
    LightingMode mode = HalfLambert;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float dir[3] = {0.0f, -1.0f, 0.0f};
    float intensity = 1.0f;

    constexpr LightingConfig() = default;
    constexpr LightingConfig(LightingMode m) : mode(m) {}
    constexpr LightingConfig(int m)
        : mode(static_cast<LightingMode>(m < 0 ? 0 : (m > 4 ? 4 : m))) {}

  };

  ModelObject() = default;
  explicit ModelObject(const LightingConfig &cfg) : initialLighting_(cfg) {}
  ~ModelObject();

  void Initialize(ID3D12Device *device);

  void SetMesh(const std::shared_ptr<ModelMesh> &mesh) { mesh_ = mesh; }
  const std::shared_ptr<ModelMesh> &GetMesh() const { return mesh_; }

  void EnsureSphericalUVIfMissing();

  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }
  void ResetTextureToMtl();

  ModelObject &SetLightingConfig(const LightingConfig &cfg) {
    initialLighting_ = cfg;
    ApplyLightingIfReady_();
    return *this;
  }
  ModelObject &SetLightingConfig(LightingMode mode,
                                 const std::array<float, 3> &color,
                                 const std::array<float, 3> &dir,
                                 float intensity);
  ModelObject &SetLightingConfig(int mode, const std::array<float, 3> &color,
                                 const std::array<float, 3> &dir,
                                 float intensity) {
    return SetLightingConfig(static_cast<LightingMode>(mode), color, dir,
                             intensity);
  }

  ModelObject &SetLightingMode(LightingMode m) {
    return SetLightingConfig(LightingConfig{m});
  }
  ModelObject &SetLightingMode(int m) {
    return SetLightingConfig(LightingConfig{m});
  }

  Transform &T() { return transform_; }
  Material *Mat() { return cbMat_.mapped; }
  void SetColor(const RC::Vector4 &color);
  DirectionalLight *Light() { return cbLight_.mapped; }

  void SetVisible(bool v) { visible_ = v; }
  bool Visible() const { return visible_; }

  void ResetBatchCursor() { cbWvpBatchHead_ = 0; }

  void SetTextureManager(TextureManager *tm) { texman_ = tm; }

  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);

  void Draw(ID3D12GraphicsCommandList *cmdList);

  void Draw(ID3D12GraphicsCommandList *cmdList, D3D12_GPU_VIRTUAL_ADDRESS lightCB);

  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances);

  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 D3D12_GPU_VIRTUAL_ADDRESS lightCB);

  void DrawImGui(const char *name, bool showLightingUi);

private:
  struct CB_WVP {
    ID3D12Resource *resource = nullptr;
    TransformationMatrix *mapped = nullptr;
  };
  struct CB_Material {
    ID3D12Resource *resource = nullptr;
    Material *mapped = nullptr;
  };
  struct CB_Light {
    ID3D12Resource *resource = nullptr;
    DirectionalLight *mapped = nullptr;
  };

  void ApplyLightingIfReady_();

private:
  ID3D12Device *device_ = nullptr;
  std::shared_ptr<ModelMesh> mesh_;

  CB_WVP cbWvp_{};
  CB_Material cbMat_{};
  CB_Light cbLight_{};

  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{};
  Transform transform_{{1, 1, 1}, {0, 0, 0}, {0, 0, 0}};
  bool visible_ = true;

  ID3D12Resource *cbWvpBatch_ = nullptr;
  uint32_t cbWvpBatchCapacity_ = 0;
  uint8_t *cbWvpBatchMapped_ = nullptr;
  uint32_t cbWvpBatchHead_ = 0;

  TextureManager *texman_ = nullptr;

  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }
  void EnsureWvpBatchCapacity_(uint32_t count);

  LightingConfig initialLighting_{};
};

// 移行用（RenderCommon側の修正が終わったら消してOK）
using Model3D = ModelObject;
