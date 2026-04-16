#pragma once
#include "RC.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

// 前方宣言
class Dx12Core;

class RenderTexture {
public:
  RenderTexture() = default;
  ~RenderTexture() = default;

  /// <summary>
  /// RenderTexture を初期化します
  /// </summary>
  void Initialize(Dx12Core *dxCore, uint32_t width, uint32_t height,
                  DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

  /// <summary>
  /// 任意の状態にリソースを遷移させます（汎用）
  /// </summary>
  void Transition(ID3D12GraphicsCommandList *cmdList,
                  D3D12_RESOURCE_STATES nextState);

  /// <summary>
  /// 描画先として使用するための状態(RenderTarget)に遷移させます
  /// </summary>
  void TransitionToRenderTarget(ID3D12GraphicsCommandList *cmdList);

  /// <summary>
  /// テクスチャとして読み込む状態(ShaderResource)に遷移させます
  /// </summary>
  void TransitionToShaderResource(ID3D12GraphicsCommandList *cmdList);

  /// <summary>
  /// リソースを明示的に解放します
  /// </summary>
  void Release() { resource_.Reset(); }

  // 各種ゲッター
  Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() const {
    return resource_;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return rtvHandle_; }
  D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPU() const { return srvHandleCPU_; }
  D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPU() const { return srvHandleGPU_; }

private:
  Microsoft::WRL::ComPtr<ID3D12Resource>
  CreateRenderTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device,
                              UINT width, UINT height, DXGI_FORMAT format,
                              const RC::Vector4 &clearColor);

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};

  D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
  D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};

  // 生成される前は COMMON (特別な意味を持たない未初期化状態) としておく
  D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;
};
