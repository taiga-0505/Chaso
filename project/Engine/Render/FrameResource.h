#pragma once

// ============================================================================
// FrameResource
// ----------------------------------------------------------------------------
// フレームごとの動的アップロードバッファ管理（リニアアロケータ）。
// GPU がフレーム N を実行中に CPU がフレーム N+1 のデータを書き込んでも
// 競合しないように、フレームごとに別々のバッファ領域を使う。
//
// 使い方:
//   1. フレーム開始時に Reset() で書き込みカーソルを先頭に戻す
//   2. 描画中は AllocCB / AllocSRV で領域を確保→書き込み
//   3. GPU はフレーム完了後に Release される
//
// RenderContext が kFrameCount 個のインスタンスをラウンドロビンで使う。
// ============================================================================

#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

namespace RC {

/// @class FrameResource
/// @brief フレームごとの動的アップロードバッファを管理するクラス（リニアアロケータ）
/// @details GPU がフレーム N を実行中に CPU がフレーム N+1 のデータを書き込んでも競合しないように、
/// フレームごとに独立したバッファ領域を提供します。
class FrameResource {
public:
  /// @brief トリプルバッファリングに対応する最大フレーム数
  static constexpr uint32_t kFrameCount = 3;

  FrameResource() = default;
  ~FrameResource() = default;

  /// @brief リソースの初期化
  /// @param device D3D12 デバイス
  /// @param frameIndex フレームインデックス（デバッグ名用）
  void Init(ID3D12Device *device, uint32_t frameIndex);

  /// @brief リソースの解放
  void Term();

  /// @brief フレーム開始時に呼び出し、書き込みカーソルを先頭にリセットする
  void Reset();

  /// @brief 定数バッファ (CB) 用のメモリを確保する
  /// @param sizeBytes 必要なバイト数
  /// @param outMapped [out] CPU側の書き込み先ポインタを受け取る変数
  /// @return GPU 仮想アドレス（SetGraphicsRootConstantBufferView 等に使用）
  /// @note 確保される領域は 256 バイトアライメントされます。
  D3D12_GPU_VIRTUAL_ADDRESS AllocCB(uint32_t sizeBytes, void **outMapped);

  /// @brief SRV / StructuredBuffer 用のメモリを確保する
  /// @param sizeBytes 必要なバイト数
  /// @param outMapped [out] CPU側の書き込み先ポインタを受け取る変数
  /// @return GPU 仮想アドレス（SetGraphicsRootShaderResourceView 等に使用）
  /// @note アライメント制限はありません。
  D3D12_GPU_VIRTUAL_ADDRESS AllocSRV(uint32_t sizeBytes, void **outMapped);

  /// @brief 定数バッファ用の残り容量があるか確認する
  /// @param sizeBytes 確保したいサイズ
  /// @return 容量が足りていれば true
  bool HasCBSpace(uint32_t sizeBytes) const;

  /// @brief SRV用の残り容量があるか確認する
  /// @param sizeBytes 確保したいサイズ
  /// @return 容量が足りていれば true
  bool HasSRVSpace(uint32_t sizeBytes) const;

private:
  /// @brief 256 バイトアライメントに切り上げる
  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }

  /// @brief CB ヒープの初期容量（1MB：WVP 約数千個分を想定）
  static constexpr uint64_t kDefaultCBSize = 1u * 1024 * 1024;
  Microsoft::WRL::ComPtr<ID3D12Resource> cbHeap_; ///< 定数バッファ用リソース
  uint8_t *cbMapped_ = nullptr;                  ///< マップされた先頭ポインタ
  uint64_t cbOffset_ = 0;                        ///< 現在の書き込みオフセット
  uint64_t cbCapacity_ = 0;                      ///< ヒープの総容量

  /// @brief SRV ヒープの初期容量（4MB：InstanceDataGPU 約数万個分を想定）
  static constexpr uint64_t kDefaultSRVSize = 4u * 1024 * 1024;
  Microsoft::WRL::ComPtr<ID3D12Resource> srvHeap_; ///< SRV/StructuredBuffer用リソース
  uint8_t *srvMapped_ = nullptr;                   ///< マップされた先頭ポインタ
  uint64_t srvOffset_ = 0;                         ///< 現在の書き込みオフセット
  uint64_t srvCapacity_ = 0;                       ///< ヒープの総容量

  Microsoft::WRL::ComPtr<ID3D12Device> device_;    ///< D3D12 デバイス
  uint32_t frameIndex_ = 0;                        ///< フレームインデックス
};

} // namespace RC
