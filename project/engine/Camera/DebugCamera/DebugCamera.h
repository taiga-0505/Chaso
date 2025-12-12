#pragma once
#include "Input/Input.h"
#include "struct.h"

class DebugCamera {
public:
  /// input: キー入力インスタンスへのポインタ
  /// fovY: 垂直画角 (ラジアン)
  /// aspect: アスペクト比 (width/height)
  /// nearZ/farZ: ニア／ファー距離
  void Initialize(Input *input, float fovY, float aspect, float nearZ,
                  float farZ);

  /// 毎フレーム呼ぶ
  void Update();

  void Reset();

  /// シェーダ等に渡すためのアクセサ
  const RC::Matrix4x4 &GetView() const { return view_; }
  const RC::Matrix4x4 &GetProjection() const { return proj_; }

  RC::Vector3 GetPosition() const { return translation_; }

private:
  Input *input_ = nullptr;           // キー入力
  RC::Vector3 rotation_ = {0, 0, 0}; // 各軸回転角
  RC::Vector3 translation_ = {0, 0, -8}; // カメラ位置
  RC::Matrix4x4 view_;                   // ビュー行列
  RC::Matrix4x4 proj_;                   // 射影行列

  float deltaTIme_ = 1.0f / 60.0f;

  const float moveSpeed_ = 0.5f;
  const float rotateSpeed_ = 0.02f;
};
