#pragma once
#include "Math/MathTypes.h"
#include <cstdint>
#include <string>
#include <vector>

struct Transform {
  RC::Vector3 scale;
  RC::Vector3 rotation;
  RC::Vector3 translation;

  Transform &operator+=(const RC::Vector3 &velocity) {
    this->translation.x += velocity.x;
    this->translation.y += velocity.y;
    this->translation.z += velocity.z;
    return *this;
  }
};

struct Emitter {
  Transform transform;
  uint32_t count;      // 一度に放出するパーティクル数
  float frequency;     // 放出間隔（秒）
  float frequencyTime; // 放出間隔タイマー
};

struct ParticleData {
  Transform transform;
  RC::Vector3 velocity;
  RC::Vector4 color;
  float lifeTime;
  float currentTime;
};

struct Segment {
  RC::Vector3 origin = {0.0f, 0.0f, 0.0f};
  RC::Vector3 diff = {1.0f, 1.0f, 1.0f};
  uint32_t color = 0x000000FF;
};

struct VertexData {
  RC::Vector4 position; // 頂点の位置
  RC::Vector2 texcoord; // テクスチャ座標
  RC::Vector3 normal;   // 法線ベクトル
};

struct MaterialData {
  std::string textureFilePath; // テクスチャファイルのパス
};

struct ModelData {
  std::vector<VertexData> vertices; // 頂点データの配列
  MaterialData material;            // マテリアルデータ
};

struct Material {
  RC::Vector4 color;         // 色 (RGBA)
  int lightingMode;          // 0:なし, 1:Lambert, 2:Half Lambert
  float padding[3];          // アラインメント調整
  RC::Matrix4x4 uvTransform; // UV変換行列
};

struct SpriteMaterial {
  RC::Vector4 color;         // 色 (RGBA)
  RC::Matrix4x4 uvTransform; // UV変換行列
};

struct TransformationMatrix {
  RC::Matrix4x4 WVP;
  RC::Matrix4x4 World;
};

struct ParticleForGPU {
  RC::Matrix4x4 WVP;
  RC::Matrix4x4 World;
  RC::Vector4 color;
};
;

struct DirectionalLight {
  RC::Vector4 color;     // 光の色 (RGBA)
  RC::Vector3 direction; // 光の方向
  float intensity;       // 光の強度
};

// -------------------------------
// 非スコープ enum: 無修飾で使える
// -------------------------------
enum LightingMode {
  None = 0,        // ライティング無し
  Lambert = 1,     // ランバート
  HalfLambert = 2, // ハーフランバート
};
