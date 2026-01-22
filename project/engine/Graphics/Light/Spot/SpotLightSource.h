#pragma once
#include "Math/MathTypes.h"
#include "struct.h"

namespace RC {

/// <summary>
/// SpotLight の操作用クラス（CPU側）
/// - 実体は struct SpotLight（GPUへ送るデータ）
/// - cosAngle は「外側のカットオフ（cos）」として扱う
/// </summary>
class SpotLightSource {
public:
  SpotLightSource();

  ::SpotLight &Data() { return data_; }
  const ::SpotLight &Data() const { return data_; }

  // 便利 setter
  void SetPosition(const Vector3 &pos) { data_.position = pos; }

  // direction は「ライトが照らす向き」
  void SetDirection(const Vector3 &dir) { data_.direction = dir; }

  void SetColor(const Vector3 &rgb, float alpha = 1.0f) {
    data_.color = {rgb.x, rgb.y, rgb.z, alpha};
  }
  void SetColor(const Vector4 &rgba) { data_.color = rgba; }

  void SetIntensity(float v) { data_.intensity = v; }

  // distance = 届く距離（0以下なら無効）
  void SetDistance(float d) { data_.distance = d; }

  // decay = 距離減衰の指数（0に近いほど緩い）
  void SetDecay(float d) { data_.decay = d; }

  // cosAngle = cutoff（例: cos(30deg)）
  void SetCosAngle(float c) { data_.cosAngle = c; }

  // 角度（度/ラジアン）から cosAngle を設定したい時用
  void SetAngleDeg(float deg);
  void SetAngleRad(float rad);

  void DrawImGui(const char *name = nullptr);

private:
  ::SpotLight data_{};
};

} // namespace RC
