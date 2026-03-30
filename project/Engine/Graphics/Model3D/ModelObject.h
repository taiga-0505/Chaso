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
#include <wrl/client.h>

class TextureManager; // 前方宣言

// ============================================================
// ModelObject
// - 1つのModelMesh(共有)を配置して描画する
// - ModelMeshが持つ Node/DrawItem を使って「Node行列を反映」して描画できる
// - Textureは「override（SetTexture）」があればそれを優先。
//   overrideが無ければ、materialIndexに応じてTextureManagerでロードして使う。
// ============================================================
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
        : mode(static_cast<LightingMode>(m < 0 ? 0 : (m > 3 ? 3 : m))) {}
  };

  ModelObject() = default;
  explicit ModelObject(const LightingConfig &cfg) : initialLighting_(cfg) {}
  ~ModelObject();

  void Initialize(ID3D12Device *device);

  void SetMesh(const std::shared_ptr<ModelMesh> &mesh) {
    mesh_ = mesh;
    // meshが変わったらMaterial SRVキャッシュは破棄
    materialSrvs_.clear();
    // overrideも一旦クリア（必要なら SetTexture で入れ直してね）
    textureSrv_ = {};
  }
  const std::shared_ptr<ModelMesh> &GetMesh() const { return mesh_; }

  void EnsureSphericalUVIfMissing();

  // overrideテクスチャ（全サブメッシュ共通でこれを使う）
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }

  // override解除して「モデルのMaterialに戻す」
  // - 1枚だけではなく、materialIndexごとに読み直す
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

  // LightManager の共通ライトCB（b1）を使いたい場合の上書き。
  // 0 を渡すと「自前の cbLight_」に戻る。
  void SetExternalLightCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    externalLightCBAddress_ = addr;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetExternalLightCBAddress() const {
    return externalLightCBAddress_;
  }

  void SetVisible(bool v) { visible_ = v; }
  bool Visible() const { return visible_; }

  void SetTextureManager(TextureManager *tm) { texman_ = tm; }

  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);

  void Draw(ID3D12GraphicsCommandList *cmdList);

  // 既存の「Transform配列インスタンス」描画
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances);

  // 「Transform配列 + 色」インスタンス描画
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 const std::vector<RC::Vector4> &colors);

  // 「Transform配列 + 単色」インスタンス描画
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 const RC::Vector4 &color);

  void DrawImGui(const char *name, bool showLightingUi);

  void ResetBatchCursor() {
    cbWvpBatchHead_ = 0;
    instanceBatchHead_ = 0;
  }

private:
  struct CB_WVP {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    TransformationMatrix *mapped = nullptr;
  };
  struct CB_Material {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    Material *mapped = nullptr;
  };
  struct CB_Light {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    DirectionalLight *mapped = nullptr;
  };

  void ApplyLightingIfReady_();

  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }
  void EnsureWvpBatchCapacity_(uint32_t count);

  void EnsureInstanceBatchCapacity_(uint32_t count);

  void EnsureMaterialSrvsLoaded_();
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvForMaterial_(uint32_t materialIndex) const;

private:
  struct InstanceDataGPU {
    RC::Matrix4x4 WVP;
    RC::Matrix4x4 World;
    RC::Matrix4x4 WorldInverseTranspose;
    RC::Vector4 color;
  };

  Microsoft::WRL::ComPtr<ID3D12Resource> instanceBatch_;
  uint32_t instanceBatchCapacity_ = 0;
  uint8_t *instanceBatchMapped_ = nullptr;
  uint32_t instanceBatchHead_ = 0;

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  std::shared_ptr<ModelMesh> mesh_;

  CB_WVP cbWvp_{};
  CB_Material cbMat_{};
  CB_Light cbLight_{};

  // override（全サブメッシュ共通）
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{};

  // materialIndexごとのSRV（overrideが無い時に使用）
  std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSrvs_;

  Transform transform_{{1, 1, 1}, {0, 0, 0}, {0, 0, 0}};
  bool visible_ = true;

  // DrawItem用：複数DrawでCBを上書きしないためのリング（upload）
  Microsoft::WRL::ComPtr<ID3D12Resource> cbWvpBatch_;
  uint32_t cbWvpBatchCapacity_ = 0;
  uint8_t *cbWvpBatchMapped_ = nullptr;
  uint32_t cbWvpBatchHead_ = 0;

  TextureManager *texman_ = nullptr;

  // 0 なら自前の cbLight_ を使用。0以外なら外部ライトCB（LightManager）を使用。
  D3D12_GPU_VIRTUAL_ADDRESS externalLightCBAddress_ = 0;

  // Updateで受け取ったカメラを保持（DrawでNodeごとにWVPを作る）
  RC::Matrix4x4 cachedView_ = {};
  RC::Matrix4x4 cachedProj_ = {};
  bool hasVP_ = false;

  LightingConfig initialLighting_{};
};
