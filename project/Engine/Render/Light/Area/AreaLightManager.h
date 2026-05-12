#pragma once
#include "Light/Area/AreaLightSource.h"
#include "struct.h"
#include <array>
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

namespace RC {

/// @brief 矩形エリアライト(Rect AreaLight)を管理するマネージャクラス
/// 最大 4 つのアクティブなエリアライトを管理し、一括して GPU (b5スロット)へ転送します。
class AreaLightManager {
public:
  /// @brief 同時に有効化可能なエリアライトの最大数
  static constexpr int kMaxActive = 4;

  /// @brief 初期化処理
  /// @param device DirectX12デバイス。定数バッファの作成に使用します。
  void Init(ID3D12Device *device);
  
  /// @brief 終了処理。確保した定数バッファなどのリソースを解放します。
  void Term();

  /// @brief 新規ライトを生成する
  /// @return 生成されたライトのハンドル（失敗時は -1）
  int Create();

  /// @brief 指定したハンドルに対応するライトを破棄する
  /// @param handle ライトハンドル
  void Destroy(int handle);

  /// @brief 単一のライトのみをアクティブにする
  /// 内部で現在のアクティブリストをクリアした後、指定したライトを追加します。
  /// @param handle アクティブにするライトのハンドル
  void SetActive(int handle);

  /// @brief 先頭のアクティブライトのハンドルを取得する
  /// @return ライトハンドル。アクティブがない場合は -1
  int GetActiveHandle() const { return (activeCount_ > 0) ? active_[0] : -1; }

  /// @brief アクティブリストをすべてクリアする
  void ClearActive();

  /// @brief アクティブリストにライトを追加する
  /// @param handle 追加するライトのハンドル
  /// @return 追加に成功（最大数未満）した場合は true
  bool AddActive(int handle);

  /// @brief アクティブリストから特定のライトを除外する
  /// @param handle 除外するライトのハンドル
  void RemoveActive(int handle);

  /// @brief 現在アクティブなライトの数を取得
  /// @return アクティブ数 (0 ～ kMaxActive)
  int GetActiveCount() const { return activeCount_; }

  /// @brief アクティブリスト内の指定インデックスのハンドルを取得
  /// @param index リスト内インデックス
  /// @return ライトハンドル
  int GetActiveHandleAt(int index) const;

  /// @brief ハンドルからライトの実体を取得
  /// @param handle ライトハンドル
  /// @return ライトソースへのポインタ。無効なハンドルの場合は nullptr
  AreaLightSource *Get(int handle);

  /// @brief ハンドルからライトの実体を取得 (const)
  /// @param handle ライトハンドル
  /// @return ライトソースへのconstポインタ。無効なハンドルの場合は nullptr
  const AreaLightSource *Get(int handle) const;

  /// @brief アクティブリストの先頭にあるライトの実体を取得
  /// アクティブがない場合はデフォルト（ハンドル0）のライトを返しようと試みます。
  /// @return アクティブなライトソースへのポインタ
  AreaLightSource *GetActive();

  /// @brief アクティブリストの先頭にあるライトの実体を取得 (const)
  /// @return アクティブなライトソースへのconstポインタ
  const AreaLightSource *GetActive() const;

  /// @brief GPU転送用定数バッファ(AreaLightsCB / b5)のGPU仮想アドレスを取得
  /// @return GPU上の仮想アドレス
  D3D12_GPU_VIRTUAL_ADDRESS GetCBAddress();

  /// @brief ImGuiによるパラメータ編集UIを表示
  /// @param handle 対象のライトハンドル
  /// @param name UIに表示するラベル
  void DrawImGui(int handle, const char *name);

private:
  /// @brief ライト管理用のスロット構造体
  struct Slot {
    bool inUse = false;           ///< 使用中フラグ
    AreaLightSource light{};      ///< ライトソース実体
  };

  /// @brief ハンドルの有効性をチェックする
  bool IsValid_(int handle) const;
  
  /// @brief 未使用スロットを検索・確保する
  int AllocSlot_();
  
  /// @brief アクティブがない場合の代替ハンドルを解決する
  int ResolveFallbackHandle_() const;

  /// @brief 定数バッファを確保する
  void EnsureCB_();
  
  /// @brief CPU側のデータをGPU定数バッファに同期（コピー）する
  /// アクティブリストに含まれるライトのみをパッキングしてGPUへ送ります。
  void SyncCB_();

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_; ///< デバイス保持
  bool initialized_ = false;                    ///< 初期化済みフラグ

  std::vector<Slot> slots_;                     ///< ライトスロット配列

  std::array<int, kMaxActive> active_{};        ///< アクティブハンドル配列
  int activeCount_ = 0;                         ///< 現在のアクティブ数

  Microsoft::WRL::ComPtr<ID3D12Resource> cb_;   ///< GPU定数バッファ
  ::AreaLightsCB *mapped_ = nullptr;            ///< マップ済みポインタ
};

} // namespace RC
