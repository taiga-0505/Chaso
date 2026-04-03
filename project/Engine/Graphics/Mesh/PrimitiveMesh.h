#pragma once
#include "struct.h"
#include "Math/MathTypes.h"
#include "function/function.h"
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

class PrimitiveMesh {
public:
  PrimitiveMesh() = default;
  ~PrimitiveMesh();

  // ModelData から GPU リソース作成
  void Initialize(ID3D12Device *device, const ModelData &data);

  // 描画（RootParam は Skydome/Model と同等想定）
  // 0:Material, 1:WVP, 2:SRV, 3:Light
  void Draw(ID3D12GraphicsCommandList *cmdList);

  void Draw(ID3D12GraphicsCommandList *cmdList, const RC::Matrix4x4 &world);

  // 外からテクスチャSRV(GPUハンドル)をセット
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle) {
    textureSrv_ = srvGPUHandle;
  }

  void DrawImGui(const char *name);

  Transform &T() { return transform_; }
  Material *Mat() { return cbMat_.mapped; }
  
  void SetVisible(bool v) { visible_ = v; }
  bool Visible() const { return visible_; }

private:
  void UploadVB_(const std::vector<VertexData> &vertices);
  void UploadIB_(const std::vector<uint32_t> &indices);

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

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  VB vb_{};
  IB ib_{};
  CB_WVP cbWvp_{};
  CB_Material cbMat_{};
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{};

  Transform transform_{{1, 1, 1}, {0, 0, 0}, {0, 0, 0}};
  bool visible_ = true;
};
