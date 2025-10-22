#pragma once
#include <numbers>
#include <cmath>

struct Vector2 {
  float x, y;
};
struct Vector3 {
  float x, y, z;
};
struct Vector4 {
  float x, y, z, w;
};

struct Matrix3x3 {
  float m[3][3];
};
struct Matrix4x4 {
  float m[4][4];
};

// easeInOutSine関数の実装
inline float easeInOutSine(float x) {
  return -(std::cos(std::numbers::pi_v<float> * x) - 1.0f) / 2.0f;
}

// 線形補間関数
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// 回転軸
enum ShaftType { X, Y, Z };
