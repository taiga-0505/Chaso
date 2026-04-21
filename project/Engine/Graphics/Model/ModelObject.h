#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "ModelMesh.h"
#include "ModelResource.h"
#include "function/function.h"
#include "struct.h"
#include <array>
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <wrl/client.h>

class TextureManager; // 前方宣言

namespace RC {
class FrameResource; // 前方宣言
}

// ============================================================
// ModelObject
// - 1つのModelMesh(共有)を配置して描画する
// - ModelMeshが持つ Node/DrawItem を使って「Node行列を反映」して描画できる
// - Textureは「override（SetTexture）」があればそれを優先。
//   overrideが無ければ、materialIndexに応じてTextureManagerでロードして使う。
//
// ★ 内部的に ModelResource（GPUリソース管理）に委譲している。
//    このクラスは Transform / 可視性 / ImGui / ライティング設定 を保持するラッパー。
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
  ~ModelObject() = default;

  void Initialize(ID3D12Device *device);

  void SetMesh(const std::shared_ptr<ModelMesh> &mesh) {
    resource_.SetMesh(mesh);
  }
  const std::shared_ptr<ModelMesh> &GetMesh() const {
    return resource_.GetMesh();
  }

  void EnsureSphericalUVIfMissing();

  // overrideテクスチャ（全サブメッシュ共通でこれを使う）
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    resource_.SetTexture(srvGPUHandle);
  }

  // override解除して「モデルのMaterialに戻す」
  // - 1枚だけではなく、materialIndexごとに読み直す
  void ResetTextureToMtl() { resource_.ResetTextureToMtl(); }

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
  Material *Mat() { return resource_.Mat(); }
  void SetColor(const RC::Vector4 &color);
  DirectionalLight *Light() { return resource_.Light(); }

  /// 環境マップ映り込み係数を設定する（非同期ロード対応）
  void SetEnvironmentCoefficient(float coeff) {
    initialEnvCoeff_ = coeff;
    if (Material *mat = resource_.Mat()) {
      mat->environmentCoefficient = coeff;
    }
  }
  float GetEnvironmentCoefficient() const { return initialEnvCoeff_; }

  // LightManager の共通ライトCB（b1）を使いたい場合の上書き。
  // 0 を渡すと「自前の cbLight_」に戻る。
  void SetExternalLightCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
    resource_.SetExternalLightCBAddress(addr);
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetExternalLightCBAddress() const {
    return resource_.GetExternalLightCBAddress();
  }

  void SetVisible(bool v) { visible_ = v; }
  bool Visible() const { return visible_; }

  void SetTextureManager(TextureManager *tm) { resource_.SetTextureManager(tm); }

  void SetFilePath(const std::string &path) { filePath_ = path; }
  const std::string &GetFilePath() const { return filePath_; }

  void SetReady(bool r) { resource_.SetReady(r); }
  bool IsReady() const { return resource_.IsReady(); }

  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);

  /// ワールド行列を外部から指定して描画する（コマンドキュー用）
  void Draw(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &world,
            RC::FrameResource &frame);

  // 既存の「Transform配列インスタンス」描画
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 RC::FrameResource &frame);

  // 「Transform配列 + 単色」インスタンス描画
  void DrawBatch(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &view,
                 const RC::Matrix4x4 &proj,
                 const std::vector<Transform> &instances,
                 const RC::Vector4 &color,
                 RC::FrameResource &frame);

  void DrawImGui(const char *name, bool showLightingUi);

  void ResetBatchCursor() { resource_.ResetBatchCursor(); }

  // ── ModelResource への直接アクセス（エンジン内部用）──
  ModelResource &Resource() { return resource_; }
  const ModelResource &Resource() const { return resource_; }

private:
  void ApplyLightingIfReady_();

private:
  // GPU リソース・描画ロジック（委譲先）
  ModelResource resource_;

  // CPU側の状態
  Transform transform_{{1, 1, 1}, {0, 0, 0}, {0, 0, 0}};
  bool visible_ = true;

  // Updateで受け取ったカメラを保持（DrawでNodeごとにWVPを作る）
  RC::Matrix4x4 cachedView_ = {};
  RC::Matrix4x4 cachedProj_ = {};
  bool hasVP_ = false;

  LightingConfig initialLighting_{};

  // 環境マップ映り込み係数（Initialize後にも適用される）
  float initialEnvCoeff_ = 0.0f;

  // ImGui用: 環境マップOFF時に前回の係数を記憶
  float lastEnvCoeff_ = 0.5f;

  std::string filePath_;
};
