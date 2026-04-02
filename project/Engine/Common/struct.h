#pragma once
#include "EngineConfig.h"
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

struct CameraForGPU {
  RC::Vector3 worldPosition; // カメラのワールド座標位置
};

struct AABB {
  RC::Vector3 min = {-1.0f, -1.0f, -1.0f};
  RC::Vector3 max = {1.0f, 1.0f, 1.0f};
};

struct Rect {
  float left;
  float right;
  float bottom;
  float top;
};

struct Emitter {
  Transform transform;
  uint32_t count;      // 一度に放出するパーティクル数
  float frequency;     // 放出間隔（秒）
  float frequencyTime; // 放出間隔タイマー
};

struct AccelerationField {
  RC::Vector3 acceleration; // 加速度ベクトル
  AABB area;
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

struct Node {
  RC::Matrix4x4 localMatrix; // ローカル変換行列
  std::string name;          // ノードの名前
  std::vector<uint32_t> meshIndices; // このノードが参照するMeshのindex配列（assimp:
                                     // node->mMeshes）
  std::vector<Node> children; // 子ノードの配列
};

struct ModelData {
  std::vector<VertexData> vertices; // 頂点データの配列
  MaterialData material;            // マテリアルデータ
  Node rootNode;
};

struct Material {
  RC::Vector4 color;         // 色 (RGBA)
  int lightingMode;          // 0:なし, 1:Lambert, 2:Half Lambert
  float shininess;           // 光沢度
  float padding[2];          // アラインメント調整
  RC::Matrix4x4 uvTransform; // UV変換行列
};

struct SpriteMaterial {
  RC::Vector4 color;         // 色 (RGBA)
  RC::Matrix4x4 uvTransform; // UV変換行列
};

struct TransformationMatrix {
  RC::Matrix4x4 WVP;
  RC::Matrix4x4 World;
  RC::Matrix4x4 worldInverseTranspose;
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

struct PointLight {
  RC::Vector4 color;    // 光の色 (RGBA)
  RC::Vector3 position; // 光の位置
  float intensity;      // 光の強度
  float radius;         // 光の届く距離
  float decay;          // 減衰率
  float padding[2];     // アラインメント調整
};

struct SpotLight {
  RC::Vector4 color;     // 光の色 (RGBA)
  RC::Vector3 position;  // 光の位置
  float intensity;       // 光の強度
  RC::Vector3 direction; // 光の方向
  float distance;        // 光の届く距離
  float decay;           // 減衰率
  float cosAngle;        // スポットライトの余弦
  float padding[2];      // アラインメント調整
};

struct AreaLight {
  RC::Vector4 color;    // rgb + a
  RC::Vector3 position; // 中心
  float intensity;

  RC::Vector3 right; // 面の右方向(正規化推奨)
  float halfWidth;   // 半幅

  RC::Vector3 up;   // 面の上方向(正規化推奨)
  float halfHeight; // 半高さ

  float range;       // 影響距離（0以下なら無効扱いでもOK）
  float decay;       // 減衰指数
  uint32_t twoSided; // 1なら両面発光
  uint32_t padding;  // 16byte合わせ
};

// =============================================
// 複数ライト用CB（Object3D: Point/Spot を最大4個）
// - HLSL側の cbuffer とレイアウトを合わせる
// - RootSig: Point=b3, Spot=b4 のまま
// =============================================

static constexpr uint32_t kMaxPointLights = 4;
static constexpr uint32_t kMaxSpotLights = 4;
static const int kMaxAreaLights = 4;

// b3 用（PointLight配列）
struct PointLightsCB {
  uint32_t count = 0;                     // 有効なライト数（0〜4）
  float padding0[3] = {0.0f, 0.0f, 0.0f}; // 16byte境界
  PointLight lights[kMaxPointLights]{};
};

// b4 用（SpotLight配列）
struct SpotLightsCB {
  uint32_t count = 0;                     // 有効なライト数（0〜4）
  float padding0[3] = {0.0f, 0.0f, 0.0f}; // 16byte境界
  SpotLight lights[kMaxSpotLights]{};
};

struct AreaLightsCB {
  uint32_t count = 0;            // areaCount
  float padding0[3] = {0, 0, 0}; // 16byte境界合わせ
  AreaLight lights[kMaxAreaLights]{};
};

// 事故防止（おすすめ）
static_assert(offsetof(AreaLightsCB, lights) == 16,
              "AreaLightsCB layout mismatch");

// -------------------------------
// 非スコープ enum: 無修飾で使える
// -------------------------------
enum LightingMode {
  None = 0,        // ライティング無し
  Lambert = 1,     // ランバート
  HalfLambert = 2, // ハーフランバート
};

// 2Dプリミティブ用: 塗りつぶし/ワイヤー
// 非スコープ enum: 無修飾で使える（Scene側で kFill / kWire だけでOK）
enum kFillMode {
  kFill = 0, // 塗りつぶし
  kWire = 1, // 枠線（ワイヤー）
};
