#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

/// @brief DirectX12のコマンド発行と同期を管理するクラス
/// コマンドキュー、リスト、アロケータ、およびフェンスによるGPU同期をラップします。
class CommandContext {
public:
  /// @brief 最大フレーム数 (ダブル/トリプルバッファリング用)
  static constexpr uint32_t kMaxFrames = 3;

  CommandContext() = default;
  ~CommandContext() { Term(); }

  /// @brief 初期化
  /// @param device DirectX12デバイス
  /// @param type コマンドリストのタイプ (DIRECT, COMPUTE等)
  /// @param frameCount フレームバッファ数
  void Init(ID3D12Device *device,
            D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            uint32_t frameCount = 2);

  /// @brief 終了処理
  void Term();

  /// @brief フレーム開始処理。アロケータのリセットとリストの記録開始を行う。
  /// @param frameIndex 現在のバックバッファインデックス
  void BeginFrame(uint32_t frameIndex);

  /// @brief フレーム終了処理。コマンドを実行し、完了待ちフェンスをシグナルする。
  void EndFrame();

  /// @brief 指定したフレームのGPU処理完了を待機する
  /// @param frameIndex 待機対象のフレームインデックス
  void WaitForFrame(uint32_t frameIndex);

  /// @brief GPUの全命令完了を待機する (Flush)
  void FlushGPU();

  /// @brief フェンスの現在の完了値を取得する
  /// @return 完了済みのフェンス値
  uint64_t GetCompletedFenceValue() const;

  /// @brief 現在のコマンドを実行し、リストをリセットして次の記録を即座に開始する
  /// @return 実行完了時に期待されるフェンス値
  uint64_t ExecuteAndReset();

  /// @brief リソースのバリア（トランジション）を発行する
  /// @param res 対象リソース
  /// @param before 遷移前の状態
  /// @param after 遷移後の状態
  void Transition(ID3D12Resource *res, D3D12_RESOURCE_STATES before,
                  D3D12_RESOURCE_STATES after);

  /// @brief コマンドキューの取得
  /// @return ID3D12CommandQueueへのポインタ
  ID3D12CommandQueue *Queue() const { return queue_.Get(); }

  /// @brief コマンドリストの取得
  /// @return ID3D12GraphicsCommandListへのポインタ
  ID3D12GraphicsCommandList *List() const { return list_.Get(); }

  /// @brief 指定インデックスのコマンドアロケータを取得
  /// @param index フレームインデックス
  /// @return ID3D12CommandAllocatorへのポインタ
  ID3D12CommandAllocator *GetAllocator(uint32_t index) const { return alloc_[index].Get(); }

  /// @brief フレームバッファ数を取得
  /// @return フレーム数
  uint32_t FrameCount() const { return frameCount_; }

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;       ///< デバイス
  D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT; ///< リストタイプ

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;  ///< コマンドキュー
  std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kMaxFrames> alloc_{}; ///< アロケータ配列
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_; ///< コマンドリスト

  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;  ///< フェンス
  HANDLE fenceEvent_ = nullptr;                ///< フェンス用イベント
  uint64_t globalFenceValue_ = 0;              ///< 次にシグナルするフェンス値
  std::array<uint64_t, kMaxFrames> frameFenceValue_{}; ///< フレーム毎の完了フェンス値

  uint32_t frameCount_ = 2;       ///< フレームバッファ数
  uint32_t currentFrameIndex_ = 0; ///< 現在の記録フレームインデックス
};
