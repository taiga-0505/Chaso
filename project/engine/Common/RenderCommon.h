#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "Math/Math.h"
#include "Scene.h"
#include "Sphere/Sphere.h"
#include <d3d12.h>
#include <string>
#include <Model3D/ModelObject.h>
#include <Model3D/ModelMesh.h>

// D3D12 GPUハンドルを返すために必要
struct D3D12_GPU_DESCRIPTOR_HANDLE;

namespace RC {

// 起動時に一度だけ（App::Init 内）
void Init(SceneContext &ctx);
void Term();

// 毎フレームのカメラ共有
void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
               const RC::Vector3 camWorldPos);

// ==============================
// ライト用
// ==============================

class Light;

int CreateLight();
void DestroyLight(int lightHandle);

void SetActiveLight(int lightHandle);
int GetActiveLightHandle();

Light *GetLightPtr(int lightHandle);

void DrawImGuiLight(int lightHandle, const char *name);

// ==============================
// モデル用
// ==============================

void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

// ロード（ハンドル制）
int LoadModel(const std::string &path);

// 描画（Transform省略形あり）
void DrawModel(int modelHandle, int texHandle);
void DrawModel(int modelHandle);

void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle = -1);
void DrawImGui3D(int modelHandle, const char *name);
// 任意解放
void UnloadModel(int modelHandle);

Transform *GetModelTransformPtr(int modelHandle);
void SetModelColor(int modelHandle, const Vector4 &color);
void SetModelLightingMode(int modelHandle, LightingMode m);

void SetModelMesh(int modelHandle, const std::string &path);

void ResetCursor(int modelHandle);

// ===============================
// 2D用
// ===============================

void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);
int LoadSprite(const std::string &path, SceneContext &ctx, bool srgb = true);
void DrawSprite(int spriteHandle);
void SetSpriteTransform(int spriteHandle, const Transform &t);
void SetSpriteColor(int spriteHandle, const Vector4 &color);
void UnloadSprite(int spriteHandle);
void SetSpriteScreenSize(int spriteHandle, float w, float h);
void DrawImGui2D(int spriteHandle, const char *name);

// ===============================
// 天球用
// ===============================

// 最小形：半径0.5, 16x16, 内向き（キューブマップ等のスカイ用）
// ※ 必ず有効なテクスチャハンドルを渡してください
int GenerateSphere(int textureHandle);

// 拡張版：パラメータ指定で生成
int GenerateSphereEx(int textureHandle = -1, float radius = 0.5f,
                     unsigned int sliceCount = 16, unsigned int stackCount = 16,
                     bool inward = true);

// 描画（texHandle を指定すると一時差し替え。省略なら生成時のテクスチャ）
void DrawSphere(int sphereHandle, int texHandle = -1);

void DrawSphereImGui(int sphereHandle, const char *name = nullptr);

// 後始末
void UnloadSphere(int sphereHandle);

// 変換・見た目
Transform *GetSphereTransformPtr(int sphereHandle);
void SetSphereColor(int sphereHandle, const Vector4 &color);
void SetSphereLightingMode(int sphereHandle, LightingMode m);

// ===============================
// 共通関数
// ===============================

int LoadTex(const std::string &path, bool srgb = true);
D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle);

// ===============================
// ブレンドモード切り替え
// ===============================
void SetBlendMode(BlendMode blendMode);
BlendMode GetBlendMode();

} // namespace RC
