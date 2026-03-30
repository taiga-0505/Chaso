#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "struct.h"

class MainCamera {
public:
  /// 初期化
  ///   pos    : カメラのワールド座標
  ///   rot    : カメラの回転 (ラジアン)
  ///   fovY   : 垂直画角 (ラジアン)
  ///   aspect : 画面アスペクト比 (width/height)
  ///   nearZ, farZ: クリップ距離
  void Initialize(const RC::Vector3 &pos, const RC::Vector3 &rot, float fovY,
                  float aspect, float nearZ, float farZ);

  /// 毎フレーム呼ぶ（位置や向きを動的に変えたいならここで）
  void Update();

  /// シェーダに渡す行列
  const RC::Matrix4x4 &GetView() const { return view_; }
  const RC::Matrix4x4 &GetProjection() const { return proj_; }

  RC::Vector3 GetPosition() const { return transform_.translation; }

  /// 外部から位置や回転を変更したいとき
  void SetPosition(const RC::Vector3 &pos) { transform_.translation = pos; }
  void SetRotation(const RC::Vector3 &rot) { transform_.rotation = rot; }

private:
  Transform transform_; // scale, rotation, translation
                        // :contentReference[oaicite:1]{index=1}
  RC::Matrix4x4 view_;
  RC::Matrix4x4 proj_;
};
