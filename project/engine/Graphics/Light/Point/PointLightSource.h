#pragma once
#include "Math/MathTypes.h"
#include "struct.h"

namespace RC {

class PointLightSource {
public:
  PointLightSource();

  // 生データ（GPUに送るstruct）
  ::PointLight &Data() { return data_; }
  const ::PointLight &Data() const { return data_; }

  // 便利setter
  void SetPosition(const Vector3 &pos) { data_.position = pos; }
  void SetColor(const Vector3 &rgb, float alpha = 1.0f) {
    data_.color = {rgb.x, rgb.y, rgb.z, alpha};
  }
  void SetColor(const Vector4 &rgba) { data_.color = rgba; }
  void SetIntensity(float v) { data_.intensity = v; }
  void SetRadius(float r) { data_.radius = r; }
  void SetDecay(float d) { data_.decay = d; }

  void DrawImGui(const char *name = nullptr);

private:
  ::PointLight data_{};
};

} // namespace RC
