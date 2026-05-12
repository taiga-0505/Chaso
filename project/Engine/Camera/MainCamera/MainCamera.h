#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "struct.h"

/// @brief ゲーム内で使用するメインカメラクラス
class MainCamera {
public:
  /// @brief 初期化
  /// @param pos カメラのワールド座標
  /// @param rot カメラの回転角 (ラジアン)
  /// @param fovY 垂直画角 (ラジアン)
  /// @param aspect 画面アスペクト比 (width/height)
  /// @param nearZ ニアクリップ距離
  /// @param farZ ファークリップ距離
  void Initialize(const RC::Vector3 &pos, const RC::Vector3 &rot, float fovY,
                  float aspect, float nearZ, float farZ);

  /// @brief 更新処理。位置や向きの変更を元に行列の再計算を行う。
  void Update();

  /// @brief ビュー行列を取得する
  /// @return ビュー行列への参照
  const RC::Matrix4x4 &GetView() const { return view_; }

  /// @brief 射影行列を取得する
  /// @return 射影行列への参照
  const RC::Matrix4x4 &GetProjection() const { return proj_; }

  /// @brief カメラの座標を取得する
  /// @return カメラ座標 (Vector3)
  RC::Vector3 GetPosition() const { return transform_.translation; }

  /// @brief カメラの位置を設定する
  /// @param pos 設定する座標
  void SetPosition(const RC::Vector3 &pos) { transform_.translation = pos; }

  /// @brief カメラの回転角を設定する
  /// @param rot 設定する回転角 (ラジアン)
  void SetRotation(const RC::Vector3 &rot) { transform_.rotation = rot; }

private:
  Transform transform_; ///< カメラのトランスフォーム情報 (座標、回転、スケール)
  RC::Matrix4x4 view_;   ///< ビュー行列
  RC::Matrix4x4 proj_;   ///< 射影行列
};
