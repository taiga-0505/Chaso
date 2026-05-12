#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>

#include "../../Common/struct.h"

class Skybox;
class TextureManager;

namespace RC {

/// @brief スカイボックス(Skybox)を管理するマネージャクラス
/// キューブマップテクスチャを使用したスカイボックスのロード、描画、および環境マップ用のSRV管理を行います。
class SkyboxManager {
public:
  /// @brief 初期化処理
  /// @param device DirectX12デバイス
  /// @param texman テクスチャマネージャ（DDSキューブマップのロードに使用）
  void Init(ID3D12Device *device, TextureManager *texman);

  /// @brief デストラクタ
  ~SkyboxManager();

  /// @brief 終了処理。管理しているすべてのスカイボックスを解放します。
  void Term();

  // ムーブ許可、コピー禁止
  SkyboxManager() = default;
  SkyboxManager(SkyboxManager &&) = default;
  SkyboxManager &operator=(SkyboxManager &&) = default;
  SkyboxManager(const SkyboxManager &) = delete;
  SkyboxManager &operator=(const SkyboxManager &) = delete;

  /// @brief DDSパスからテクスチャロードとSkybox生成を一括実行する
  /// @param ddsPath DDS形式のキューブマップテクスチャパス
  /// @return スカイボックスハンドル（失敗時は -1）
  int Create(const std::string &ddsPath);

  /// @brief 指定したハンドルに対応するスカイボックスを解放する
  /// @param handle スカイボックスハンドル
  void Unload(int handle);

  /// @brief 有効なハンドルか確認する
  /// @param handle スカイボックスハンドル
  /// @return 有効なら true
  bool IsValid(int handle) const;

  /// @brief ハンドルからスカイボックスの実体を取得する
  /// @param handle スカイボックスハンドル
  /// @return Skyboxへのポインタ。無効なハンドルの場合は nullptr
  Skybox *Get(int handle);
  
  /// @brief ハンドルからスカイボックスの実体を取得する (const)
  /// @param handle スカイボックスハンドル
  /// @return Skyboxへのconstポインタ
  const Skybox *Get(int handle) const;

  /// @brief ハンドルからトランスフォーム情報ポインタを取得する
  /// @param handle スカイボックスハンドル
  /// @return Transform構造体へのポインタ
  Transform *GetTransformPtr(int handle);

  /// @brief スカイボックスの乗算カラー（明るさや色味）を設定する
  /// @param handle スカイボックスハンドル
  /// @param color 設定するRGBAカラー
  void SetColor(int handle, const Vector4 &color);

  /// @brief テクスチャのバインドを適用する
  /// @param handle スカイボックスハンドル
  void ApplyTexture(int handle);

  /// @brief キューブマップのGPUディスクリプタハンドルを取得する
  /// 環境マップやリフレクション用のテクスチャとして使用する際に呼び出します。
  /// @param handle スカイボックスハンドル
  /// @return GPUディスクリプタハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrv(int handle) const;

private:
  /// @brief スカイボックス保持用スロット
  struct Slot {
    std::unique_ptr<Skybox> ptr;           ///< スカイボックス実体
    bool inUse = false;                   ///< 使用中フラグ
    D3D12_GPU_DESCRIPTOR_HANDLE texSrv{}; ///< キューブマップのSRV
  };

  /// @brief 未使用スロットを検索・確保する
  int AllocSlot_();

private:
  ID3D12Device *device_ = nullptr;     ///< デバイス
  TextureManager *texman_ = nullptr;   ///< テクスチャマネージャ
  std::vector<Slot> skyboxes_;         ///< スロット配列
};

} // namespace RC
