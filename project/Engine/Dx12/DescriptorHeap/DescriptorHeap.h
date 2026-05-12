#pragma once
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>


/// @brief DirectX12のデスクリプタヒープを管理するクラス
/// RTV, DSV, CBV/SRV/UAVなどのデスクリプタの確保とハンドル管理を行います。
class DescriptorHeap {
public:
  /// @brief 初期化
  /// @param device DirectX12デバイス
  /// @param type ヒープのタイプ (RTV, DSV, CBV_SRV_UAV等)
  /// @param capacity 最大デスクリプタ数
  /// @param shaderVisible シェーダーから参照可能にするか (CBV_SRV_UAV等でtrue)
  void Init(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity, bool shaderVisible = false);

  /// @brief 破棄
  void Term();

  /// @brief デスクリプタを確保し、先頭のCPUハンドルを返す
  /// @param count 確保する連続したデスクリプタ数
  /// @return 確保された領域の先頭CPUハンドル
  D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPU(UINT count = 1);

  /// @brief 指定インデックスのCPUハンドルを取得
  /// @param index デスクリプタのインデックス
  /// @return CPUハンドル
  D3D12_CPU_DESCRIPTOR_HANDLE CPUAt(UINT index) const;

  /// @brief 指定インデックスのGPUハンドルを取得
  /// @param index デスクリプタのインデックス
  /// @return GPUハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE GPUAt(UINT index) const;

  /// @brief デスクリプタ1つあたりの増分サイズを取得
  /// @return インクリメントサイズ (バイト)
  UINT Increment() const { return inc_; }

  /// @brief ID3D12DescriptorHeapのポインタを取得
  /// @return ヒープポインタ
  ID3D12DescriptorHeap *Heap() const { return heap_.Get(); }

  /// @brief シェーダー参照可能かどうかを取得
  /// @return 参照可能ならtrue
  bool ShaderVisible() const { return shaderVisible_; }

  /// @brief ヒープの最大容量を取得
  /// @return 最大デスクリプタ数
  UINT Capacity() const { return capacity_; }

  /// @brief 現在の使用数を取得
  /// @return 使用済みデスクリプタ数
  UINT Used() const { return offset_; }

  /// @brief 使用状況をリセットする
  /// @note 以前確保したハンドルは無効化されるため、フレーム毎の使い捨てヒープなどで使用します。
  void Reset() { offset_ = 0; }

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;     ///< デバイス
  D3D12_DESCRIPTOR_HEAP_TYPE type_{};               ///< ヒープタイプ
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_; ///< デスクリプタヒープ本体
  UINT capacity_ = 0;   ///< 最大容量
  UINT inc_ = 0;        ///< インクリメントサイズ
  UINT offset_ = 0;     ///< 現在のオフセット
  bool shaderVisible_ = false; ///< シェーダー参照フラグ
  D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_{}; ///< 先頭CPUハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_{}; ///< 先頭GPUハンドル
};
