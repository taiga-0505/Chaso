#pragma once
#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl/client.h>

/// @brief DirectX12デバイスとDXGI関連リソースを管理するクラス
/// グラフィックスカードの列挙、デバイスの生成、デバッグレイヤーの設定などを行います。
class Device {
public:
  /// @brief 初期化
  /// @param enableDebug D3D12 デバッグレイヤーを有効にするか
  /// @param gpuValidation GPU バリデーション（実行時負荷が高い）を有効にするか
  void Init(bool enableDebug = true, bool gpuValidation = false);

  /// @brief 破棄
  void Term();

  /// @brief DXGIファクトリを取得
  /// @return IDXGIFactory6へのポインタ
  IDXGIFactory6 *Factory() const { return factory_.Get(); }

  /// @brief DXGIアダプタを取得
  /// @return IDXGIAdapter4へのポインタ
  IDXGIAdapter4 *Adapter() const { return adapter_.Get(); }

  /// @brief D3D12デバイスを取得
  /// @return ID3D12Deviceへのポインタ
  ID3D12Device *GetDevice() const { return device_.Get(); }

  /// @brief 機能レベル (Feature Level) を取得
  /// @return D3D_FEATURE_LEVEL
  D3D_FEATURE_LEVEL FeatureLevel() const { return featureLevel_; }

  /// @brief 機能レベルを文字列として取得 (例: "12.1")
  /// @return 文字列ポインタ
  const char *FeatureLevelString() const;

  /// @brief テアリング (可変リフレッシュレート) がサポートされているか確認
  /// @return サポートされていればtrue
  bool IsTearingSupported() const;

  /// @brief InfoQueueの設定 (デバッグビルド時)
  /// @param breakOnError エラー発生時にプログラムを中断するか
  /// @param muteInfo 情報ログを抑制するか
  void SetupInfoQueue(bool breakOnError = true,
                      bool muteInfo = true);

private:
  /// @brief デバッグレイヤーとGPUバリデーションの有効化
  void enableDebug_(bool gpuValidation);
  /// @brief 高性能なハードウェアアダプタを選択
  IDXGIAdapter4 *pickHardwareAdapter_();
  /// @brief WARPアダプタ (ソフトウェアレンダリング) を取得
  IDXGIAdapter4 *getWarpAdapter_();

private:
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory_; ///< DXGIファクトリ
  Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_; ///< DXGIアダプタ
  Microsoft::WRL::ComPtr<ID3D12Device> device_;   ///< D3D12デバイス
  D3D_FEATURE_LEVEL featureLevel_ = (D3D_FEATURE_LEVEL)0; ///< 決定された機能レベル
};
