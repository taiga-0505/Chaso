#include "SpotLightSource.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace RC {

static float DegToRad_(float deg) { return deg * 3.1415926535f / 180.0f; }

SpotLightSource::SpotLightSource() {
  data_.color = {1, 1, 1, 1};
  data_.position = {0.0f, 2.0f, 0.0f};
  data_.intensity = 1.0f;

  // デフォルトは「前方 -Z 方向」に照らす想定（必要なら好きに）
  data_.direction = {0.0f, -1.0f, 0.0f};

  data_.distance = 15.0f;
  data_.decay = 2.0f;

  // 30度くらいのカットオフ
  data_.cosAngle = std::cos(DegToRad_(30.0f));
}

void SpotLightSource::SetAngleDeg(float deg) {
  deg = std::clamp(deg, 0.0f, 89.9f);
  data_.cosAngle = std::cos(DegToRad_(deg));
}

void SpotLightSource::SetAngleRad(float rad) {
  rad = std::clamp(rad, 0.0f, 3.1415926535f * 0.499f);
  data_.cosAngle = std::cos(rad);
}

::SpotLight SpotLightSource::DataForGPU() const {
  ::SpotLight out = data_;
  if (!enabled_) {
    out.intensity = 0.0f;
  }
  return out;
}

} // namespace RC
