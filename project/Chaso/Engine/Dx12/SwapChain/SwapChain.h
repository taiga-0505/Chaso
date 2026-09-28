#pragma once
#include <array>
#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

/// @brief DXGIスワップチェインを管理するクラス
/// 画面表示のためのダブル/トリプルバッファリング、フリップ、リサイズを制御します。
class SwapChain {
public:
  /// @brief 最大フレーム数 (バックバッファの最大数)
  static constexpr UINT kMaxFrames = 3;

  /// @brief 初期化
  /// @param factory DXGIファクトリ
  /// @param device DirectX12デバイス
  /// @param queue 描画に使用するコマンドキュー
  /// @param hwnd ウィンドウハンドル
  /// @param width 画面幅
  /// @param height 画面高さ
  /// @param format バッファのピクセルフォーマット
  /// @param frameCount バッファ数 (2=ダブル, 3=トリプル)
  /// @param allowTearing テアリング（VRR等）を許可するか
  void Init(IDXGIFactory6 *factory, ID3D12Device *device,
            ID3D12CommandQueue *queue, HWND hwnd, UINT width, UINT height,
            DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
            UINT frameCount = 2, bool allowTearing = true);

  /// @brief 終了処理
  void Term();

  /// @brief RTVを配置するデスクリプタヒープを設定する
  /// @param heap RTV用デスクリプタヒープへのポインタ
  /// @param increment デスクリプタの増分サイズ
  void SetRtvHeap(ID3D12DescriptorHeap *heap, UINT increment);

  /// @brief ウィンドウサイズに合わせてバッファをリサイズする
  /// @param width 新しい幅
  /// @param height 新しい高さ
  void Resize(UINT width, UINT height);

  /// @brief バッファをフリップして画面を表示する
  /// @param syncInterval 垂直同期 (0=なし, 1=あり)
  /// @param flags プレゼントフラグ (DXGI_PRESENT_ALLOW_TEARING 等)
  void Present(UINT syncInterval = 1, UINT flags = 0);

  /// @brief 現在のバックバッファインデックスを取得
  /// @return インデックス (0 ~ frameCount-1)
  UINT CurrentBackBufferIndex() const {
    return swap_->GetCurrentBackBufferIndex();
  }

  /// @brief 指定インデックスのバックバッファリソースを取得
  /// @param i インデックス
  /// @return ID3D12Resourceへのポインタ
  ID3D12Resource *BackBuffer(UINT i) const { return backBuffers_[i].Get(); }

  /// @brief 指定インデックスのRTVハンドルを取得
  /// @param i インデックス
  /// @return D3D12_CPU_DESCRIPTOR_HANDLE
  D3D12_CPU_DESCRIPTOR_HANDLE RtvAt(UINT i) const { return rtv_[i]; }

  /// @brief バッファ数を取得
  /// @return フレーム数
  UINT FrameCount() const { return frameCount_; }

  /// @brief フォーマットを取得
  /// @return DXGI_FORMAT
  DXGI_FORMAT Format() const { return format_; }

  /// @brief IDXGISwapChain4ポインタを直接取得
  /// @return IDXGISwapChain4へのポインタ
  IDXGISwapChain4 *Raw() const {
    return swap_.Get();
  }

private:
  /// @brief スワップチェイン本体の生成
  void createSwapChain(HWND hwnd);

  /// @brief バックバッファ用のRTVを生成
  void createBackBufferRTVs();

private:
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;   ///< DXGIファクトリ（非所有）
  Microsoft::WRL::ComPtr<ID3D12Device> device_;     ///< デバイス（非所有）
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_; ///< コマンドキュー（非所有）
  Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_;    ///< スワップチェイン本体

  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFrames> backBuffers_{}; ///< バックバッファリソース配列
  std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxFrames> rtv_{}; ///< RTVハンドル配列

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_; ///< RTV用デスクリプタヒープ
  UINT rtvInc_ = 0; ///< RTVデスクリプタの増分サイズ

  UINT width_ = 0, height_ = 0; ///< 解像度
  DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM; ///< フォーマット
  UINT frameCount_ = 2; ///< バッファ数
  bool allowTearing_ = true; ///< テアリング許可フラグ
};
