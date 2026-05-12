#pragma once
#include "Input/Input.h"
#include "struct.h"

/// @brief デバッグ用の自由移動カメラクラス
class DebugCamera {
public:
  /// @brief 初期化
  /// @param input キー入力インスタンスへのポインタ
  /// @param fovY 垂直画角 (ラジアン)
  /// @param aspect アスペクト比 (width/height)
  /// @param nearZ ニアクリップ距離
  /// @param farZ ファークリップ距離
  void Initialize(Input *input, float fovY, float aspect, float nearZ,
                  float farZ);

  /// @brief 更新処理。入力に基づいてカメラの移動・回転・行列計算を行う。
  void Update();

  /// @brief 位置と回転を初期状態にリセットする
  void Reset();

  /// @brief ビュー行列を取得する
  /// @return ビュー行列への参照
  const RC::Matrix4x4 &GetView() const { return view_; }

  /// @brief 射影行列を取得する
  /// @return 射影行列への参照
  const RC::Matrix4x4 &GetProjection() const { return proj_; }

  /// @brief カメラの座標を取得する
  /// @return カメラ座標 (Vector3)
  RC::Vector3 GetPosition() const { return translation_; }

  /// @brief カメラの位置を設定する
  /// @param pos 設定する座標
  void SetPosition(const RC::Vector3 &pos);

  /// @brief カメラの回転角を設定する
  /// @param rot 設定する回転角 (ラジアン)
  void SetRotation(const RC::Vector3 &rot);

  /// @brief カメラの位置と回転を同時に設定する
  /// @param pos 設定する座標
  /// @param rot 設定する回転角 (ラジアン)
  void SetTransform(const RC::Vector3 &pos, const RC::Vector3 &rot);

private:
  /// @brief ビュー行列の再計算を行う内部関数
  void RebuildView_();

  Input *input_ = nullptr;               ///< キー入力インスタンスへのポインタ
  RC::Vector3 rotation_ = {0, 0, 0};     ///< 各軸の回転角 (ラジアン)
  RC::Vector3 translation_ = {0, 0, -8}; ///< カメラの位置 (並進座標)
  RC::Matrix4x4 view_;                   ///< ビュー行列
  RC::Matrix4x4 proj_;                   ///< 射影行列

  float deltaTime_ = 1.0f / 60.0f;       ///< 1フレームあたりの経過時間

  const float moveSpeed_ = 2.0f;         ///< カメラの移動速度
  const float rotateSpeed_ = 1.0f;       ///< カメラの回転速度
};
