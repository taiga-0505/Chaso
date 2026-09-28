#pragma once
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
#include "SRVManager/SRVManager.h"

class SRVManager;

/// @brief StructuredBuffer（構造化バッファ）を管理するクラス
/// Uploadヒープ上へのリソース確保と、SRVManagerを用いた記述子管理を統合します。
class StructuredBufferManager {
public:
  /// @brief バッファを識別するためのID型
  using BufferID = int;

  /// @brief 初期化
  /// @param srvMgr SRV管理クラスへのポインタ
  void Init(SRVManager *srvMgr);

  /// @brief 終了処理。全リソースを解放します。
  void Term();

  /// @brief 新規バッファの作成
  /// @param elementCount 要素数
  /// @param strideBytes 1要素あたりのバイトサイズ
  /// @return 生成されたバッファのID
  BufferID Create(UINT elementCount, UINT strideBytes);

  /// @brief CPUから書き込むためにバッファをマップする
  /// @param id 対象バッファのID
  /// @return マップされたメモリへのポインタ
  void *Map(BufferID id);

  /// @brief バッファのアンマップ
  /// @param id 対象バッファのID
  void Unmap(BufferID id);

  /// @brief SRVのGPUハンドルを取得
  /// @param id 対象バッファのID
  /// @return D3D12_GPU_DESCRIPTOR_HANDLE
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(BufferID id) const;

  /// @brief リソース本体を取得
  /// @param id 対象バッファのID
  /// @return ID3D12Resourceへのポインタ
  ID3D12Resource *GetResource(BufferID id) const;

  /// @brief 指定したバッファを破棄する
  /// @param id 対象バッファのID
  void Destroy(BufferID id);

private:
  /// @brief 管理用のエントリ構造体
  struct Entry {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource; ///< バッファリソース本体
    void *mapped = nullptr;                         ///< マップされたアドレス

    // SRVManager が管理するスロット
    SRVManager::Handle srvHandle{};                 ///< SRVManagerが管理する記述子ハンドル

    UINT stride = 0;                                ///< ストライドサイズ
    UINT count = 0;                                 ///< 要素数
  };

  SRVManager *srvMgr_ = nullptr;                    ///< SRVマネージャ（非所有）
  Microsoft::WRL::ComPtr<ID3D12Device> device_;     ///< デバイス（非所有）
  std::vector<Entry> entries_;                     ///< バッファエントリの配列
};
