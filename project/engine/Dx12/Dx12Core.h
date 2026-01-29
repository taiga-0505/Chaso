#pragma once
#include "CommandContext/CommandContext.h"
#include "DepthStencil/DepthStencil.h"
#include "DescriptorHeap/DescriptorHeap.h"
#include "Device/Device.h"
#include "FixFps/FixFps.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "SRVManager/SRVManager.h"
#include "StructuredBufferManager/StructuredBufferManager.h"
#include "SwapChain/SwapChain.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>

/// <summary>
/// DX12 のデバイス/コマンド/スワップチェーン管理を統括する
/// </summary>
class Dx12Core {
public:
  /// <summary>
  /// Dx12Core 初期化に使用する設定を保持する
  /// </summary>
  struct Desc {
    UINT width = 1280;
    UINT height = 720;
    DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    UINT frameCount = 2;
    bool debug = true;
    bool gpuValidation = false;
    UINT srvHeapCapacity = 256;
    bool allowTearingIfSupported = true;
  };

  /// <summary>
  /// DX12 コアを初期化する
  /// </summary>
  void Init(HWND hwnd, const Desc &d);

  /// <summary>
  /// DX12 コアを終了する
  /// </summary>
  void Term();

  /// <summary>
  /// FPS 固定の有効/無効を切り替える
  /// </summary>
  void EnableFixFps(bool enable = true);

  /// <summary>
  /// フレーム開始処理を行う
  /// </summary>
  void BeginFrame(); // フレーム開始（allocator/list reset を内包）

  /// <summary>
  /// フレーム終了処理を行う
  /// </summary>
  void EndFrame();   // Close→Execute→Present（Fence発行まで）

  /// <summary>
  /// GPU の完了を待機する
  /// </summary>
  void WaitForGPU(); // Flush（終了時など）

  /// <summary>
  /// 現在のバックバッファインデックスを取得する
  /// </summary>
  UINT BackBufferIndex() const { return backIndex_; }

  /// <summary>
  /// D3D12 デバイスを取得する
  /// </summary>
  ID3D12Device *GetDevice() const { return device_.GetDevice(); }

  /// <summary>
  /// コマンドリストを取得する
  /// </summary>
  ID3D12GraphicsCommandList *CL() const { return cmd_.List(); }

  /// <summary>
  /// コマンドキューを取得する
  /// </summary>
  ID3D12CommandQueue *Queue() const { return cmd_.Queue(); }

  /// <summary>
  /// SRV 用ディスクリプタヒープを取得する
  /// </summary>
  DescriptorHeap &SRV() { return srv_; }

  /// <summary>
  /// RTV 用ディスクリプタヒープを取得する
  /// </summary>
  DescriptorHeap &RTV() { return rtv_; }

  /// <summary>
  /// DSV 用ディスクリプタヒープを取得する
  /// </summary>
  DescriptorHeap &DSV() { return dsv_; }

  /// <summary>
  /// 構造化バッファ管理を取得する
  /// </summary>
  StructuredBufferManager &StructuredBuffers() { return sbMgr_; }

  /// <summary>
  /// 現在の RT の CPU ハンドルを取得する
  /// </summary>
  D3D12_CPU_DESCRIPTOR_HANDLE CurrentRTV() const {
    return swap_.RtvAt(backIndex_);
  }

  /// <summary>
  /// 深度ステンシルの CPU ハンドルを取得する
  /// </summary>
  D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const { return depth_.Dsv(); }

  /// <summary>
  /// SRV 管理を取得する
  /// </summary>
  SRVManager &SRVMan() { return srvMgr_; }

  /// <summary>
  /// SRV 管理を取得する（const）
  /// </summary>
  const SRVManager &SRVMan() const { return srvMgr_; }

  /// <summary>
  /// 画面をクリアする
  /// </summary>
  void Clear(float r = 0.1f, float g = 0.25f, float b = 0.5f, float a = 1.0f);

  /// <summary>
  /// フレーム数を取得する
  /// </summary>
  UINT FrameCount() const { return swap_.FrameCount(); }

  /// <summary>
  /// ビューポートを設定する
  /// </summary>
  void SetViewport(const D3D12_VIEWPORT &vp) { viewport_ = vp; }

  /// <summary>
  /// シザー矩形を設定する
  /// </summary>
  void SetScissor(const D3D12_RECT &rc) { scissor_ = rc; }

  /// <summary>
  /// ビューポートを取得する
  /// </summary>
  const D3D12_VIEWPORT &Viewport() const { return viewport_; }

  /// <summary>
  /// シザー矩形を取得する
  /// </summary>
  const D3D12_RECT &Scissor() const { return scissor_; }

  /// <summary>
  /// バックバッファサイズに合わせてビューポートとシザーをリセットする
  /// </summary>
  void ResetViewportScissorToBackbuffer(UINT width, UINT height) {
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissor_.left = 0;
    scissor_.top = 0;
    scissor_.right = static_cast<LONG>(width);
    scissor_.bottom = static_cast<LONG>(height);
  }

private:
  Desc desc_{};
  Device device_;
  CommandContext cmd_;
  SwapChain swap_;
  DescriptorHeap rtv_, dsv_, srv_;
  SRVManager srvMgr_;
  DepthStencil depth_;
  UINT backIndex_ = 0;
  bool allowTearing_ = false;
  D3D12_VIEWPORT viewport_{};
  D3D12_RECT scissor_{};
  std::unique_ptr<FixFps> fixFps_;
  bool fixFpsEnabled_ = true;
  StructuredBufferManager sbMgr_;
};
