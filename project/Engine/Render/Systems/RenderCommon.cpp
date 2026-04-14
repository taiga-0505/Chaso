// ============================================================================
// RenderCommon.cpp
// ----------------------------------------------------------------------------
// RenderCommon は「Scene から呼べる描画ファサード。」
//
// 責務分割後、このファイルに残るもの:
//   - RenderContext の唯一のインスタンス管理
//   - Init / Term
//   - SetCamera
//   - PreDraw3D / PreDraw2D
//   - Texture (LoadTex / GetSrv)
//   - BlendMode (Set / Get)
//   - IsInitialized / GetDevice
//
// Model / Sprite / Sphere / Light / Primitive の実装は
// 各 Render*.cpp で RC::名前空間の関数として定義される。
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "Dx12/Dx12Core.h"
#include "PipelineManager.h"
#include "Primitive/Primitive2D.h"
#include "Primitive/Primitive3D.h"
#include "Scene.h"

#include <string_view>

namespace RC {

// ============================================================================
// Init / Term
// ============================================================================

void Init(SceneContext &ctx) { RenderContext::GetInstance().Init(ctx); }

void Term() { RenderContext::GetInstance().Term(); }

// ============================================================================
// Camera
// ============================================================================

void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
               const RC::Vector3 camWorldPos) {
  RenderContext::GetInstance().SetCamera(view, proj, camWorldPos);
}

// ============================================================================
// 3D Pass
// ============================================================================

void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RenderContext::GetInstance().PreDraw3D(ctx, cl);
}

// ============================================================================
// 2D Pass
// ============================================================================

void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RenderContext::GetInstance().PreDraw2D(ctx, cl);
}

// ============================================================================
// Texture
// ============================================================================

int LoadTex(const std::string &path, bool srgb) {
  return RenderContext::GetInstance().LoadTex(path, srgb);
}

D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle) {
  return RenderContext::GetInstance().GetSrv(texHandle);
}

// ============================================================================
// State
// ============================================================================

bool IsInitialized() { return RenderContext::GetInstance().IsInitialized(); }

ID3D12Device *GetDevice() { return RenderContext::GetInstance().Device(); }

void SetBlendMode(BlendMode blendMode) {
  RenderContext::GetInstance().SetBlendMode(blendMode);
}

BlendMode GetBlendMode() {
  return RenderContext::GetInstance().CurrentBlendMode();
}

} // namespace RC
