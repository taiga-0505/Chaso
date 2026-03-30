#pragma once
#include "Math/MathTypes.h"
#include "Math/MathUtils.h" // SafeNormalize / Clamp など使う
#include <algorithm>
#include <cmath>

namespace RC::CameraMath {

// ここはあなたのエンジンのカメラと揃える前提：
// yaw = atan2(x, z) なので「+Zが前」系
inline Vector3 LookAtYawPitch(const Vector3 &eye, const Vector3 &target,
                              const Vector3 &fallbackForward = Vector3{0, 0,
                                                                       1}) {
  const Vector3 dir = RC::SafeNormalize(RC::Sub(target, eye), fallbackForward);

  const float yaw = std::atan2(dir.x, dir.z);
  const float pitch = std::asin(std::clamp(dir.y, -1.0f, 1.0f));

  return Vector3{pitch, yaw, 0.0f}; // rollは使わない想定
}

// yaw/pitch から forward を作る（DebugCamera でよく使う）
inline Vector3 ForwardFromYawPitch(float yaw, float pitch) {
  const float cy = std::cos(yaw);
  const float sy = std::sin(yaw);
  const float cp = std::cos(pitch);
  const float sp = std::sin(pitch);

  // 「+Zが前」系の forward
  return Vector3{sy * cp, sp, cy * cp};
}

inline Vector3 ForwardFromRotation(const Vector3 &rot) {
  return ForwardFromYawPitch(rot.y, rot.x); // rot = {pitch, yaw, roll} の前提
}

} // namespace RC::CameraMath
