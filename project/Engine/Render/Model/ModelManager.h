#pragma once

// ModelManager
// -----------------------------------------------------------------------------
// 「モデル周り」の管理クラス
// - モデルはハンドル制（int）
// - 同じ .obj は Mesh を共有（weak_ptr キャッシュ）
// - ModelObject 生成時に TextureManager を注入して mtl テクスチャも維持
// -----------------------------------------------------------------------------

#include <d3d12.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <future>

#include "struct.h"

class ModelObject;
class ModelMesh;

class TextureManager; // 前方宣言

namespace RC {

class ModelManager {
public:
  /// <summary>
  /// 初期化（device/texman を注入）
  /// </summary>
  void Init(ID3D12Device *device, TextureManager *texman);

  ~ModelManager();

  /// <summary>
  /// 終了（管理しているモデルと Mesh キャッシュを解放）
  /// </summary>
  void Term();

  /// <summary>
  /// モデルをロードしてハンドルを返す（Mesh は共有キャッシュ）
  /// </summary>
  int Load(const std::string &path);

  /// <summary>
  /// モデルを解放（ハンドルは無効化）
  /// </summary>
  void Unload(int handle);

  /// <summary>
  /// 有効ハンドルか？
  /// </summary>
  bool IsValid(int handle) const;

  /// <summary>
  /// モデル実体を取得（無効なら nullptr）
  /// </summary>
  ::ModelObject *Get(int handle);
  const ::ModelObject *Get(int handle) const;

  /// <summary>
  /// Transform ポインタ取得
  /// </summary>
  Transform *GetTransformPtr(int handle);

  /// <summary>
  /// 乗算カラー設定
  /// </summary>
  void SetColor(int handle, const Vector4 &color);

  /// <summary>
  /// ライティングモード設定
  /// </summary>
  void SetLightingMode(int handle, LightingMode m);

  /// <summary>
  /// Mesh 差し替え（同じ .obj は共有）
  /// </summary>
  void SetMesh(int handle, const std::string &path);

  /// <summary>
  /// DrawModelBatch の内部カーソルをリセット
  /// </summary>
  void ResetCursor(int handle);

  /// <summary>
  /// 全モデルのバッチカーソルを毎フレームリセット（PreDraw3D から呼ぶ想定）
  /// </summary>
  void ResetAllBatchCursors();

private:
  struct Slot {
    std::unique_ptr<::ModelObject> ptr;
    bool inUse = false;
  };

  int AllocSlot_();

  std::shared_ptr<::ModelMesh> GetOrLoadMesh_(const std::string &path);

private:
  ID3D12Device *device_ = nullptr;
  TextureManager *texman_ = nullptr;

  std::vector<Slot> models_;
  std::unordered_map<std::string, std::weak_ptr<::ModelMesh>> meshCache_;

  mutable std::recursive_mutex mtx_;
  std::unordered_map<std::string, std::shared_future<void>> loadingTasks_;
};

} // namespace RC
