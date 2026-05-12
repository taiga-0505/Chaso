#pragma once
#include "RC.h"
#include <d3d12.h>
#include <type_traits>
#include <wrl.h>

namespace RC {

/// @brief ワールド変換行列の定数バッファ用データ構造体
struct ConstBufferDataWorldTransform {
  Matrix4x4 matWorld; ///< ワールド変換行列
};

/// @brief オブジェクトのワールド行列（位置・回転・スケール）を管理するクラス
class WorldTransform {
public:
  /// @brief 初期化
  void Initialize();

  /// @brief 位置・回転・スケールからワールド行列を更新する。親があれば合成する。
  void UpdateMatrix();

  /// @brief 定数バッファの生成
  void CreateConstBuffer();

  /// @brief 定数バッファのメモリマッピング
  void Map();

  /// @brief 更新したワールド行列を定数バッファへ転送する
  void TransferMatrix();

  /// @brief 定数バッファリソースの取得
  /// @return 定数バッファリソースへのComPtr
  const Microsoft::WRL::ComPtr<ID3D12Resource> &GetConstBuffer() const {
    return constBuffer_;
  }

  /// @brief ワールド行列からワールド座標（並進成分）を取得する
  /// @return ワールド座標 (Vector3)
  Vector3 GetWorldPosition() const {
    return {matWorld_.m[3][0], matWorld_.m[3][1], matWorld_.m[3][2]};
  }

  /// @brief スケールを取得する
  /// @return スケール (Vector3)
  Vector3 GetScale() const { return scale_; }

  /// @brief スケールを設定する
  /// @param s 設定するスケール
  void SetScale(const Vector3 &s) { scale_ = s; }

  /// @brief 回転角を取得する
  /// @return 回転角 (Vector3, ラジアン)
  Vector3 GetRotation() const { return rotation_; }

  /// @brief 回転角を設定する
  /// @param r 設定する回転角 (ラジアン)
  void SetRotation(const Vector3 &r) { rotation_ = r; }

  /// @brief ローカル座標（並進）を取得する
  /// @return ローカル座標 (Vector3)
  Vector3 GetTranslation() const { return translation_; }

  /// @brief ローカル座標（並進）を設定する
  /// @param t 設定する座標
  void SetTranslation(const Vector3 &t) { translation_ = t; }

  /// @brief 計算済みのワールド行列を取得する
  /// @return ワールド行列
  const Matrix4x4 &GetMatWorld() const { return matWorld_; }

  /// @brief 親となるWorldTransformを設定する
  /// @param parent 親のポインタ。nullptrで解除。
  void SetParent(const WorldTransform *parent) { parent_ = parent; }

private:
  Vector3 scale_ = {1, 1, 1};       ///< スケール
  Vector3 rotation_ = {0, 0, 0};    ///< 回転角 (ラジアン)
  Vector3 translation_ = {0, 0, 0}; ///< 座標 (並進)

  Matrix4x4 matWorld_{};                 ///< ローカル・ワールド変換行列
  const WorldTransform *parent_ = nullptr; ///< 親オブジェクトへのポインタ

  Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_; ///< 定数バッファ
  ConstBufferDataWorldTransform *constMap = nullptr;   ///< マッピング済みのアドレス

  // コピー禁止
  WorldTransform(const WorldTransform &) = delete;
  WorldTransform &operator=(const WorldTransform &) = delete;
};

static_assert(!std::is_copy_assignable_v<WorldTransform>);

} // namespace RC
