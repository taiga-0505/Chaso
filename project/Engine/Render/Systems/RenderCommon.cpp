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
#include "Graphics/PostProcess/PostProcess.h"
#include "Scene.h"
#include "imgui/imgui.h"

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

void SetViewShadingMode(ViewShadingMode mode) {
  RenderContext::GetInstance().SetViewShadingMode(mode);
}

ViewShadingMode GetViewShadingMode() {
  return RenderContext::GetInstance().GetViewShadingMode();
}

void DrawViewShadingModeImGui(const char *label) {
  int current = static_cast<int>(GetViewShadingMode());
  const char *items[] = {"Solid", "Wireframe", "Solid + Wireframe"};
  if (ImGui::Combo(label, &current, items, 3)) {
    SetViewShadingMode(static_cast<ViewShadingMode>(current));
  }
}

void SetPostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->SetEffect(type);
  }
}

::PostEffectType GetPostEffect() {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    return pp->GetEffect();
  }
  return ::PostEffectType::None;
}

void AddPostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->AddEffect(type);
  }
}

void RemovePostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->RemoveEffect(type);
  }
}

void ClearPostEffects() {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->ClearEffects();
  }
}

bool HasPostEffect(::PostEffectType type) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    return pp->HasEffect(type);
  }
  return false;
}

void DrawPostEffectImGui(const char *label) {
  if (PostProcess *pp = RenderContext::GetInstance().GetPostProcess()) {
    pp->DrawImGui(label);
  }
}

void AddLoadingTask(std::future<void> &&task) {
  RenderContext::GetInstance().AddLoadingTask(std::move(task));
}

void WaitAllLoads() { RenderContext::GetInstance().WaitAllLoads(); }

void ClearTextureLogHistory() {
  RenderContext::GetInstance().Textures().ClearLogHistory();
}

} // namespace RC
