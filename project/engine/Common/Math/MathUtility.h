#pragma once
#include "MathTypes.h"
#include <cfloat>
#include <cmath>

// 小さな値の判定に使う共通EPS
constexpr float MU_EPS = 1e-6f;

//==================================
// Vector3 演算子オーバーロード
//==================================

// 加算・減算
inline Vector3 operator+(const Vector3 &a, const Vector3 &b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vector3 operator-(const Vector3 &a, const Vector3 &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// スカラー倍／除算
inline Vector3 operator*(const Vector3 &v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}
inline Vector3 operator*(float s, const Vector3 &v) { return v * s; }
inline Vector3 operator/(const Vector3 &v, float s) {
  // s ~ 0 の安全対策
  const float d = (std::fabs(s) < MU_EPS) ? (s >= 0 ? MU_EPS : -MU_EPS) : s;
  return {v.x / d, v.y / d, v.z / d};
}

// アダマール積（要るなら）: 要件が「成分ごとの積」なら明示
inline Vector3 hadamard(const Vector3 &a, const Vector3 &b) {
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}

// 複合代入
inline Vector3 &operator+=(Vector3 &a, const Vector3 &b) {
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}
inline Vector3 &operator-=(Vector3 &a, const Vector3 &b) {
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
  return a;
}
inline Vector3 &operator*=(Vector3 &v, float s) {
  v.x *= s;
  v.y *= s;
  v.z *= s;
  return v;
}
inline Vector3 &operator/=(Vector3 &v, float s) {
  const float d = (std::fabs(s) < MU_EPS) ? (s >= 0 ? MU_EPS : -MU_EPS) : s;
  v.x /= d;
  v.y /= d;
  v.z /= d;
  return v;
}
