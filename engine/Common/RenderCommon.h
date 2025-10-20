#pragma once
#include <d3d12.h>
#include <string>
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "Math/Math.h"
#include "Scene.h"

// D3D12 GPUハンドルを返すために必要
struct D3D12_GPU_DESCRIPTOR_HANDLE;

namespace RC {

// 起動時に一度だけ（App::Init 内）
void Init(SceneContext &ctx);
void Term();

// 毎フレームのカメラ共有
void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj);

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
// 共通関数
// ===============================

int LoadTex(const std::string &path, bool srgb = true);
D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle);

// ===============================
// ブレンドモード切り替え
// ===============================
void SetBlendMode(BlendMode blendMode);
BlendMode GetBlendMode();


} // namespace RenderCommon
