#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "ModelMesh.h"
#include "function/function.h"
#include "struct.h"
#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl/client.h>

class TextureManager; // 前方宣言

namespace RC {
class FrameResource; // 前方宣言
}

// ============================================================
// ModelResource
// - GPU リソース（CB, SRV, バッファ）と描画ロジックを管理する
// - ModelObject から分離された「GPU側のデータ」を担当
// - 1つの ModelMesh (共有) をバインドし、Draw/DrawBatch を実行する
//
// ★ 毎フレームの WVP CB / InstanceData は FrameResource (リニア
//    アロケータ) から確保するため、CPU/GPU 間のレースコンディション
//    が発生しない。
// ============================================================
class ModelResource {
public:
  ModelResource() = default;
  ~ModelResource();

  /// GPU リソースの初期化（Device 必須）
  void Initialize(ID3D12Device *device);

  // ── Mesh 管理 ──
  void SetMesh(const std::shared_ptr<ModelMesh> &mesh);
  const std::shared_ptr<ModelMesh> &GetMesh() const { return mesh_; }

  // ── テクスチャ ──
  /// 全サブメッシュ共通のoverrideテクスチャを設定
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }
  /// overrideを解除して materialIndex ごとのSRVに戻す
  void ResetTextureToMtl();

  void SetTextureManager(TextureManager *tm) { texman_ = tm; }

  // ── Material / Light CB アクセス ──
  Material *Mat() { return cbMat_.mapped; }
  DirectionalLight *Light() { return cbLight_.mapped; }

  void SetExternalLightCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    externalLightCBAddress_ = addr;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetExternalLightCBAddress() const {
    return externalLightCBAddress_;
  }

  // ── ライティング初期設定 ──
  void ApplyLighting(int lightingMode, const float color[3],
                     const float dir[3], float intensity);

  // ── 描画 ──
  /// ワールド行列を外部から指定して描画する
  void Draw(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &world,
            const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj,
            RC::FrameResource &frame);

  /// インスタンシング描画（白色）
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 RC::FrameResource &frame);

  /// インスタンシング描画（単色指定）
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 const RC::Vector4 &color,
                 RC::FrameResource &frame);

  /// バッチカーソルをリセット（互換用：FrameResource 化により実質ノーオプ）
  void ResetBatchCursor() { /* FrameResource 側で管理 */ }

  // ── 状態問い合わせ ──
  bool IsReady() const { return isReady_; }
  void SetReady(bool r) { isReady_ = r; }

private:
  // ── GPU定数バッファ ──
  struct CB_Material {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    Material *mapped = nullptr;
  };
  struct CB_Light {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    DirectionalLight *mapped = nullptr;
  };

  // ── インスタンスデータ (GPU) ──
  struct InstanceDataGPU {
    RC::Matrix4x4 WVP;
    RC::Matrix4x4 World;
    RC::Matrix4x4 WorldInverseTranspose;
    RC::Vector4 color;
  };

  // ── ヘルパー ──
  void EnsureMaterialSrvsLoaded_();
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvForMaterial_(uint32_t materialIndex) const;

  // ── メンバ ──
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  std::shared_ptr<ModelMesh> mesh_;

  CB_Material cbMat_{};
  CB_Light cbLight_{};

  // override（全サブメッシュ共通）
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{};

  // materialIndexごとのSRV（overrideが無い時に使用）
  std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSrvs_;

  // ライトCB（外部 or 自前）
  D3D12_GPU_VIRTUAL_ADDRESS externalLightCBAddress_ = 0;

  TextureManager *texman_ = nullptr;
  bool isReady_ = false;
};
