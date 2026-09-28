#pragma once

#include <d3d12.h>
#include <memory>
#include <vector>

#include "../../Common/struct.h"

class Skydome;
class TextureManager;

namespace RC {

/// @brief スカイドーム(Skydome)を管理するマネージャクラス
/// 球体メッシュとパノラマテクスチャ（または背景テクスチャ）を使用したスカイドームを管理します。
class SkydomeManager {
public:
  /// @brief 初期化処理
  /// @param device DirectX12デバイス
  /// @param texman テクスチャマネージャ（テクスチャハンドルの解決に使用）
  void Init(ID3D12Device *device, TextureManager *texman);

  /// @brief デストラクタ
  ~SkydomeManager();

  /// @brief 終了処理。管理しているすべてのスカイドームを解放します。
  void Term();

  /// @brief 新規スカイドームを生成する
  /// @param textureHandle 使用するテクスチャのハンドル
  /// @param radius 球体の半径
  /// @param sliceCount 経度方向の分割数
  /// @param stackCount 緯度方向の分割数
  /// @return スカイドームハンドル（失敗時は -1）
  int Create(int textureHandle, float radius = 100.0f, unsigned int sliceCount = 32,
             unsigned int stackCount = 32);

  /// @brief 指定したハンドルに対応するスカイドームを解放する
  /// @param handle スカイドームハンドル
  void Unload(int handle);

  /// @brief 有効なハンドルか確認する
  /// @param handle スカイドームハンドル
  /// @return 有効なら true
  bool IsValid(int handle) const;

  /// @brief ハンドルからスカイドームの実体を取得する
  /// @param handle スカイドームハンドル
  /// @return Skydomeへのポインタ。無効なハンドルの場合は nullptr
  Skydome *Get(int handle);
  
  /// @brief ハンドルからスカイドームの実体を取得する (const)
  /// @param handle スカイドームハンドル
  /// @return Skydomeへのconstポインタ
  const Skydome *Get(int handle) const;

  /// @brief ハンドルからトランスフォーム情報ポインタを取得する
  /// @param handle スカイドームハンドル
  /// @return Transform構造体へのポインタ
  Transform *GetTransformPtr(int handle);

  /// @brief スカイドームの乗算カラーを設定する
  /// @param handle スカイドームハンドル
  /// @param color 設定するRGBAカラー
  void SetColor(int handle, const Vector4 &color);

  /// @brief スカイドームに使用するテクスチャを上書き適用する
  /// @param handle スカイドームハンドル
  /// @param overrideTexHandle 新しいテクスチャハンドル
  void ApplyTexture(int handle, int overrideTexHandle);

private:
  /// @brief スカイドーム保持用スロット
  struct Slot {
    std::unique_ptr<Skydome> ptr; ///< スカイドーム実体
    bool inUse = false;          ///< 使用中フラグ
    int defaultTexHandle = -1;   ///< デフォルトのテクスチャハンドル
  };

  /// @brief 未使用スロットを検索・確保する
  int AllocSlot_();

private:
  ID3D12Device *device_ = nullptr;     ///< デバイス
  TextureManager *texman_ = nullptr;   ///< テクスチャマネージャ
  std::vector<Slot> skydomes_;         ///< スロット配列
};

} // namespace RC
