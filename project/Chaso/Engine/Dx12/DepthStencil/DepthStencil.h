#pragma once
#include <cassert>
#include <d3d12.h>
#include <wrl/client.h>

class DescriptorHeap; // 前方宣言（ヘッダ循環防止）

/// @brief 深度ステンシルバッファ（テクスチャとDSV）を管理するクラス
class DepthStencil {
public:
  /// @brief 初期化。深度ステンシルリソースの作成とDSVの配置を行う。
  /// @param device DirectX12デバイス
  /// @param width 幅
  /// @param height 高さ
  /// @param dsvHeap DSVを配置するデスクリプタヒープ
  /// @param texFormat テクスチャリソースのフォーマット
  /// @param dsvFormat DSVのフォーマット
  void Init(ID3D12Device *device, UINT width, UINT height,
            DescriptorHeap &dsvHeap,
            DXGI_FORMAT texFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
            DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT);

  /// @brief バッファのリサイズ
  /// @note GPUのアイドル待機後に呼び出す必要があります。
  /// @param width 新しい幅
  /// @param height 新しい高さ
  /// @param dsvHeap DSVを再配置するデスクリプタヒープ
  void Resize(UINT width, UINT height, DescriptorHeap &dsvHeap);

  /// @brief 破棄
  void Term();

  /// @brief 深度ステンシルバッファのクリア
  /// @param list コマンドリスト
  /// @param depth クリアする深度値 (0.0 ~ 1.0)
  /// @param stencil クリアするステンシル値
  void Clear(ID3D12GraphicsCommandList *list, float depth = 1.0f,
             UINT8 stencil = 0) const;

  /// @brief DSVのCPUハンドルを取得
  /// @return D3D12_CPU_DESCRIPTOR_HANDLE
  D3D12_CPU_DESCRIPTOR_HANDLE Dsv() const { return dsv_; }

  /// @brief 深度ステンシルリソースの取得
  /// @return ID3D12Resourceへのポインタ
  ID3D12Resource *Resource() const { return tex_.Get(); }

  /// @brief テクスチャフォーマットの取得
  /// @return DXGI_FORMAT
  DXGI_FORMAT Format() const { return texFormat_; }

  /// @brief 幅を取得
  /// @return 幅
  UINT Width() const { return width_; }

  /// @brief 高さを取得
  /// @return 高さ
  UINT Height() const { return height_; }

private:
  /// @brief 深度ステンシルリソースの生成
  void createResource();
  /// @brief 深度ステンシルビュー(DSV)の生成
  void createView(DescriptorHeap &dsvHeap);

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_; ///< デバイス
  Microsoft::WRL::ComPtr<ID3D12Resource> tex_;  ///< 深度ステンシルテクスチャリソース
  D3D12_CPU_DESCRIPTOR_HANDLE dsv_{};           ///< デスクリプタヒープ内のDSVハンドル
  UINT width_ = 0, height_ = 0;                 ///< サイズ
  DXGI_FORMAT texFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT; ///< テクスチャフォーマット
  DXGI_FORMAT dsvFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT; ///< DSVフォーマット
};
