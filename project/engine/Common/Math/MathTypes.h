#pragma once

struct Vector2 {
  float x, y;
};

struct Vector2Int { // グリッド座標のために追加
	int x, y;

	// 比較演算子
	bool operator==(const Vector2Int& other) const {
		return x == other.x && y == other.y;
	}
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

// 回転軸
enum ShaftType { X, Y, Z };
