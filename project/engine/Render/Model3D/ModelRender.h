#pragma once
#include <string>
#include <vector>

#include "struct.h"

namespace RC {

// ============================================================================
// Model / Sphere API (RC::xxx)
// ----------------------------------------------------------------------------
// RenderCommon から Model 関連の責務を完全に分離したモジュール。
// - 呼び出し側は今まで通り RC::LoadModel / RC::DrawModel / ... を使える
// - 実装と依存（ModelManager / SphereManager / ModelObject 等）はこの .cpp に閉じる
// ============================================================================

// ------------------------------
// Model
// ------------------------------
int LoadModel(const std::string &path);
void UnloadModel(int modelHandle);

void DrawModel(int modelHandle, int texHandle);
void DrawModel(int modelHandle);
void DrawModelNoCull(int modelHandle, int texHandle = -1);

void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle = -1);

void DrawImGui3D(int modelHandle, const char *name);

Transform *GetModelTransformPtr(int modelHandle);
void SetModelColor(int modelHandle, const Vector4 &color);
void SetModelLightingMode(int modelHandle, LightingMode m);
void SetModelMesh(int modelHandle, const std::string &path);
void ResetCursor(int modelHandle);

// ------------------------------
// Sphere (Sky dome)
// ------------------------------
int GenerateSphere(int textureHandle);

int GenerateSphereEx(int textureHandle = -1, float radius = 0.5f,
                     unsigned int sliceCount = 16,
                     unsigned int stackCount = 16, bool inward = true);

void DrawSphere(int sphereHandle, int texHandle = -1);
void DrawSphereImGui(int sphereHandle, const char *name = nullptr);

void UnloadSphere(int sphereHandle);
Transform *GetSphereTransformPtr(int sphereHandle);
void SetSphereColor(int sphereHandle, const Vector4 &color);
void SetSphereLightingMode(int sphereHandle, LightingMode m);

// ============================================================================
// ModelRender (internal)
// ----------------------------------------------------------------------------
// 描画実装をまとめるためのヘルパー。
// 外からは基本的に free function の RC::DrawModel 等を使う。
// ============================================================================
class ModelRender {
public:
  // texHandle: -1 の場合は mtl のテクスチャを使用
  static void DrawModel(int modelHandle, int texHandle = -1);

  // カリング無効版（texHandle: -1 の場合は mtl を使用）
  static void DrawModelNoCull(int modelHandle, int texHandle = -1);

  // インスタンシング描画（texHandle: -1 の場合は mtl を使用）
  static void DrawModelBatch(int modelHandle,
                             const std::vector<Transform> &instances,
                             int texHandle = -1);

  // Sphere 描画（texHandle: -1 の場合は生成時/保持しているテクスチャ）
  static void DrawSphere(int sphereHandle, int texHandle = -1);
};

} // namespace RC
