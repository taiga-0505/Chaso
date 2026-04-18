#pragma once
#include "Math/MathTypes.h"
#include "function/function.h"
#include "struct.h"
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

class Skybox {
public:
  Skybox() = default;
  ~Skybox();

  // Device依存リソースの作成 + ボックスメッシュ生成
  void Initialize(ID3D12Device *device);

  // 毎フレームの行列更新（外部カメラの View/Proj を渡す）
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);

  // 描画（RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light）
  void Draw(ID3D12GraphicsCommandList *cmdList);

  /// <summary>
  /// ワールド行列を外部から指定して描画する（コマンドキュー用）
  /// </summary>
  void Draw(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &world);

  void DrawImGui(const char *name = nullptr);

  // 外からテクスチャSRV(GPUハンドル)をセット
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }

  // 外から調整したいとき用のアクセサ
  Transform &T() { return transform_; }
  Material *Mat() { return cbMat_.mapped; }

  void SetVisible(bool v) { visible_ = v; }
  bool Visible() const { return visible_; }

private:
  // ボックスメッシュ生成 (内側を向く)
  void BuildBox_();
  void UploadVB_();
  void UploadIB_();

  struct VB {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_VERTEX_BUFFER_VIEW view{};
    uint32_t vertexCount = 0;
  };
  struct IB {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_INDEX_BUFFER_VIEW view{};
    uint32_t indexCount = 0;
  };
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

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  // CPU側バッファ
  std::vector<VertexData> vertices_;
  std::vector<uint16_t> indices_;

  // GPUリソース
  VB vb_{};
  IB ib_{};
  CB_WVP cbWvp_{};
  CB_Material cbMat_{};
  CB_Light cbLight_{};
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{};

  // 変換
  Transform transform_{{100, 100, 100}, {0, 0, 0}, {0, 0, 0}};

  bool visible_ = true;
};
