#pragma once
#include "MathTypes.h" // RC::Vector3 とかが入ってる想定
#include <algorithm>
#include <cmath>

namespace RC {

// ----- scalar helpers -----
inline float Clamp(float v, float mn, float mx) {
  return std::clamp(v, mn, mx);
}
inline float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// 「sharpness」と「dt」から指数補間の係数を作るやつ
// camPos = Lerp(camPos, desired, ExpSmoothingFactor(sharpness, dt));
inline float ExpSmoothingFactor(float sharpness, float dt) {
  if (sharpness <= 0.0f)
    return 1.0f;
  if (dt <= 0.0f)
    return 0.0f;
  return 1.0f - std::exp(-sharpness * dt);
}

// ----- Vector3 basic ops (operatorが無い前提の保険) -----
inline Vector3 Add(const Vector3 &a, const Vector3 &b) {
  return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vector3 Sub(const Vector3 &a, const Vector3 &b) {
  return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vector3 Mul(const Vector3 &v, float s) {
  return Vector3{v.x * s, v.y * s, v.z * s};
}

inline float Dot(const Vector3 &a, const Vector3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float LengthSq(const Vector3 &v) { return Dot(v, v); }
inline float Length(const Vector3 &v) { return std::sqrt(LengthSq(v)); }

// 0除算回避つきNormalize（ゼロベクトルなら fallback を返す）
inline Vector3 SafeNormalize(const Vector3 &v,
                             const Vector3 &fallback = Vector3{0, 0, 1}) {
  const float lsq = LengthSq(v);
  if (lsq <= 1e-12f)
    return fallback;
  const float inv = 1.0f / std::sqrt(lsq);
  return Vector3{v.x * inv, v.y * inv, v.z * inv};
}

inline Vector3 Lerp(const Vector3 &a, const Vector3 &b, float t) {
  return Vector3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t};
}

} // namespace RC
