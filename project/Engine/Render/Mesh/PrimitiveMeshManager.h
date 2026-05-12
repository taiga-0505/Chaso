#pragma once
#include <d3d12.h>
#include <memory>
#include <vector>
#include "struct.h"

class PrimitiveMesh;
class TextureManager;

namespace RC {

/// @brief 動的に生成されたプリミティブメッシュを管理するクラス
/// ハンドル制でメッシュオブジェクトを保持し、テクスチャの適用やトランスフォームの管理を行います。
class PrimitiveMeshManager {
public:
  /// @brief 初期化処理
  /// @param device DirectX12デバイス
  /// @param texman テクスチャマネージャ（テクスチャハンドルの解決に使用）
  void Init(ID3D12Device *device, TextureManager *texman);

  /// @brief デストラクタ
  ~PrimitiveMeshManager();

  /// @brief 終了処理。管理しているすべてのプリミティブメッシュを解放します。
  void Term();

  /// @brief 新規プリミティブメッシュを生成する
  /// @param data モデルデータ（頂点・インデックス配列など）
  /// @param textureHandle 使用するテクスチャのハンドル（-1でデフォルト）
  /// @param typeName オブジェクトの識別名（デバッグ・UI表示用）
  /// @return メッシュハンドル（失敗時は -1）
  int Create(const ModelData &data, int textureHandle = -1, const std::string &typeName = "Unknown");

  /// @brief 指定したハンドルに対応するプリミティブメッシュを解放する
  /// @param handle メッシュハンドル
  void Unload(int handle);

  /// @brief 有効なハンドルか確認する
  /// @param handle メッシュハンドル
  /// @return 有効なら true
  bool IsValid(int handle) const;

  /// @brief ハンドルからプリミティブメッシュの実体を取得する
  /// @param handle メッシュハンドル
  /// @return PrimitiveMeshへのポインタ。無効なハンドルの場合は nullptr
  PrimitiveMesh *Get(int handle);
  
  /// @brief ハンドルからプリミティブメッシュの実体を取得する (const)
  /// @param handle メッシュハンドル
  /// @return PrimitiveMeshへのconstポインタ
  const PrimitiveMesh *Get(int handle) const;

  /// @brief ハンドルからトランスフォーム情報ポインタを取得する
  /// @param handle メッシュハンドル
  /// @return Transform構造体へのポインタ
  Transform *GetTransformPtr(int handle);

  /// @brief メッシュの乗算カラーを設定する
  /// @param handle メッシュハンドル
  /// @param color 設定するRGBAカラー
  void SetColor(int handle, const Vector4 &color);

  /// @brief メッシュに使用するテクスチャを上書き適用する
  /// @param handle メッシュハンドル
  /// @param overrideTexHandle 新しいテクスチャハンドル
  void ApplyTexture(int handle, int overrideTexHandle);

  /// @brief ImGuiによる編集UIを表示する
  /// @param handle メッシュハンドル
  /// @param name UIに表示するラベル
  void DrawImGui(int handle, const char *name);

private:
  /// @brief メッシュオブジェクト保持用スロット
  struct Slot {
    std::unique_ptr<PrimitiveMesh> ptr; ///< メッシュ実体
    bool inUse = false;                ///< 使用中フラグ
    int defaultTexHandle = -1;         ///< デフォルトのテクスチャハンドル
    std::string typeName;              ///< 型名・識別名
  };

  /// @brief 未使用スロットを検索・確保する
  int AllocSlot_();

private:
  ID3D12Device *device_ = nullptr;     ///< デバイス
  TextureManager *texman_ = nullptr;   ///< テクスチャマネージャ
  D3D12_GPU_DESCRIPTOR_HANDLE whiteSrv_{}; ///< テクスチャ未指定時の白テクスチャ用SRV
  std::vector<Slot> primitives_;       ///< スロット配列
};

} // namespace RC
