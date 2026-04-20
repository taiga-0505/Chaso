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

class FrameResource {
public:
  /// フレーム数（トリプルバッファ対応）
  static constexpr uint32_t kFrameCount = 3;

  FrameResource() = default;
  ~FrameResource() = default;

  /// 初期化（frameIndex はデバッグ名用）
  void Init(ID3D12Device *device, uint32_t frameIndex);

  /// フレーム開始時にカーソルリセット
  void Reset();

  /// CB 用アロケーション（256B アライメント）
  /// @param sizeBytes 必要なバイト数
  /// @param outMapped 書き込み先ポインタ
  /// @return GPU 仮想アドレス（SetGraphicsRootConstantBufferView 用）
  D3D12_GPU_VIRTUAL_ADDRESS AllocCB(uint32_t sizeBytes, void **outMapped);

  /// SRV / StructuredBuffer 用アロケーション（アライメントなし）
  /// @param sizeBytes 必要なバイト数
  /// @param outMapped 書き込み先ポインタ
  /// @return GPU 仮想アドレス（SetGraphicsRootShaderResourceView 用）
  D3D12_GPU_VIRTUAL_ADDRESS AllocSRV(uint32_t sizeBytes, void **outMapped);

  /// 容量不足チェック
  bool HasCBSpace(uint32_t sizeBytes) const;
  bool HasSRVSpace(uint32_t sizeBytes) const;

private:
  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }

  // CB ヒープ（256B aligned 定数バッファ用）
  // 初期サイズ: 1MB（WVP × 数千個分を想定）
  static constexpr uint64_t kDefaultCBSize = 1u * 1024 * 1024;
  Microsoft::WRL::ComPtr<ID3D12Resource> cbHeap_;
  uint8_t *cbMapped_ = nullptr;
  uint64_t cbOffset_ = 0;
  uint64_t cbCapacity_ = 0;

  // SRV ヒープ（インスタンスデータ等、アライメント不要）
  // 初期サイズ: 4MB（InstanceDataGPU × 数万個分を想定）
  static constexpr uint64_t kDefaultSRVSize = 4u * 1024 * 1024;
  Microsoft::WRL::ComPtr<ID3D12Resource> srvHeap_;
  uint8_t *srvMapped_ = nullptr;
  uint64_t srvOffset_ = 0;
  uint64_t srvCapacity_ = 0;

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  uint32_t frameIndex_ = 0;
};

} // namespace RC
