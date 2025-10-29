#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <windows.h>
#include "Math/MathTypes.h"

// printf関数の表示位置
static const int kRowHeight = 20;
static const int kColumnWidth = 60;

struct Transform {
  RC::Vector3 scale;
  RC::Vector3 rotation;
  RC::Vector3 translation;
};

// 球
struct SphereData {
  RC::Vector3 center = {0.0f, 0.0f, 0.0f};
  float radius = 1.0f;
  uint32_t color = 0x000000FF;
};

// 三角形
struct Triangle2D {
  RC::Vector3 vertices[3] = {
      {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
  uint32_t color = 0x000000FF;
};

// 軸並行境界箱 (Axis-Aligned Bounding Box)
struct AABB {
  RC::Vector3 min = {-1.0f, -1.0f, -1.0f};
  RC::Vector3 max = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

// 線
// origin: 始点
// diff: 終点

struct Line {
  RC::Vector3 origin = {0.0f, 0.0f, 0.0f};
  RC::Vector3 diff = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

struct Ray {
  RC::Vector3 origin = {0.0f, 0.0f, 0.0f};
  RC::Vector3 diff = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

struct Segment {
  RC::Vector3 origin = {0.0f, 0.0f, 0.0f};
  RC::Vector3 diff = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

// 平面
struct Plane {
  RC::Vector3 normal = {0.0f, 1.0f, 0.0f};
  float distance = 1.0f;
  uint32_t color = 0xFFFFFFFF;
};

struct VertexData {
  RC::Vector4 position; // 頂点の位置
  RC::Vector2 texcoord; // テクスチャ座標
  RC::Vector3 normal;   // 法線ベクトル
};

struct Material {
  RC::Vector4 color;     // 色 (RGBA)
  int lightingMode;      // 0:なし, 1:Lambert, 2:Half Lambert
  float padding[3];      // アラインメント調整
  RC::Matrix4x4 uvTransform; // UV変換行列
};

struct TransformationMatrix {
  RC::Matrix4x4 WVP;
  RC::Matrix4x4 World;
};

struct DirectionalLight {
  RC::Vector4 color; // 光の色 (RGBA)
  RC::Vector3 direction; // 光の方向
  float intensity;   // 光の強度
};

struct MaterialData {
  std::string textureFilePath; // テクスチャファイルのパス
};

struct ModelData {
  std::vector<VertexData> vertices; // 頂点データの配列
  MaterialData material;            // マテリアルデータ
};
