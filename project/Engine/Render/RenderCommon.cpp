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
#include "Primitive2D/Primitive2D.h"
#include "Primitive3D/Primitive3D.h"
#include "Scene.h"

#include <string_view>

namespace RC {

// ============================================================================
// 唯一の RenderContext インスタンス
// ============================================================================

static RenderContext s_ctx;

RenderContext &GetRenderContext() { return s_ctx; }

// ============================================================================
// Init / Term
// ============================================================================

void Init(SceneContext &ctx) { s_ctx.Init(ctx); }

void Term() { s_ctx.Term(); }

// ============================================================================
// Camera
// ============================================================================

void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
               const RC::Vector3 camWorldPos) {
  s_ctx.SetCamera(view, proj, camWorldPos);
}

// ============================================================================
// 3D Pass
// ============================================================================

void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  s_ctx.SetCommandList(cl);
  s_ctx.SetSceneContext(&ctx);
  s_ctx.SetBlendMode(kBlendModeNone);

  auto *pso = s_ctx.GetPipeline("object3d", kBlendModeNone);
  if (!pso) {
    s_ctx.SetCommandList(nullptr);
    s_ctx.SetSceneContext(nullptr);
    return;
  }

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  s_ctx.BindCameraCB();
  s_ctx.BindAllLightCBs();

  s_ctx.Models().ResetAllBatchCursors();

  if (auto *prim = s_ctx.EnsurePrimitive3D()) {
    prim->BeginFrame(s_ctx.View(), s_ctx.Proj(), kBlendModeNone);
  }
}

// ============================================================================
// 2D Pass
// ============================================================================

void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  s_ctx.SetCommandList(cl);
  s_ctx.SetSceneContext(&ctx);
  s_ctx.SetBlendMode(kBlendModeNormal);

  // 3D の Primitive3D を先にフラッシュ
  if (auto *prim = s_ctx.EnsurePrimitive3D()) {
    if (prim->HasAny()) {
      const BlendMode saved = s_ctx.CurrentBlendMode();
      s_ctx.SetBlendMode(prim->BlendAt3D());

      if (s_ctx.BindPipeline("primitive3d")) {
        cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        prim->Draw(cl, true);
      }
      if (s_ctx.BindPipeline("primitive3d_nodepth")) {
        cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        prim->Draw(cl, false);
      }

      s_ctx.SetBlendMode(saved);
    }
  }

  // 2D Viewport / Scissor
  D3D12_VIEWPORT viewport{};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<float>(ctx.app->width);
  viewport.Height = static_cast<float>(ctx.app->height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  D3D12_RECT scissor{};
  scissor.left = 0;
  scissor.top = 0;
  scissor.right = static_cast<LONG>(ctx.app->width);
  scissor.bottom = static_cast<LONG>(ctx.app->height);

  cl->RSSetViewports(1, &viewport);
  cl->RSSetScissorRects(1, &scissor);

  auto *pso = s_ctx.GetPipeline("sprite", kBlendModeNormal);
  if (!pso) {
    s_ctx.SetCommandList(nullptr);
    s_ctx.SetSceneContext(nullptr);
    return;
  }

  // Primitive2D の BeginFrame
  if (auto *prim = s_ctx.EnsurePrimitive2D()) {
    prim->BeginFrame();
  }

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ============================================================================
// Texture
// ============================================================================

int LoadTex(const std::string &path, bool srgb) {
  if (!s_ctx.IsInitialized()) {
    return -1;
  }
  return s_ctx.Textures().LoadID(path, srgb);
}

D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle) {
  if (!s_ctx.IsInitialized() || texHandle < 0) {
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }
  return s_ctx.Textures().GetSrv(texHandle);
}

// ============================================================================
// State
// ============================================================================

bool IsInitialized() { return s_ctx.IsInitialized(); }

ID3D12Device *GetDevice() { return s_ctx.Device(); }

void SetBlendMode(BlendMode blendMode) { s_ctx.SetBlendMode(blendMode); }

BlendMode GetBlendMode() { return s_ctx.CurrentBlendMode(); }

} // namespace RC
