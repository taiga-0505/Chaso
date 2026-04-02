// ============================================================================
// RenderPrimitive.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の Primitive2D / Primitive3D / Fog API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "PipelineManager.h"
#include "Primitive/Primitive2D.h"
#include "Primitive/Primitive3D.h"
#include "Scene.h"

namespace RC {

// ============================================================================
// Primitive2D（即時描画）
// ============================================================================

void DrawLine(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
              float thickness, float feather) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  if (!ctx.BindPipeline("primitive2d")) {
    return;
  }

  auto *prim = ctx.EnsurePrimitive2D();
  if (!prim) {
    return;
  }

  prim->SetLine(pos1, pos2, thickness, color, feather);
  prim->Draw(ctx.CL());
}

void DrawBox(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
             kFillMode fillMode, float feather) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  if (!ctx.BindPipeline("primitive2d")) {
    return;
  }

  auto *prim = ctx.EnsurePrimitive2D();
  if (!prim) {
    return;
  }

  float thickness = 1.0f;
  prim->SetRect(pos1, pos2, fillMode, thickness, color, feather);
  prim->Draw(ctx.CL());
}

void DrawCircle(const Vector2 &center, float radius, const Vector4 &color,
                kFillMode fillMode, float feather) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  if (!ctx.BindPipeline("primitive2d")) {
    return;
  }

  auto *prim = ctx.EnsurePrimitive2D();
  if (!prim) {
    return;
  }

  float thickness = 1.0f;
  prim->SetCircle(center, radius, fillMode, thickness, color, feather);
  prim->Draw(ctx.CL());
}

void DrawTriangle(const Vector2 &pos1, const Vector2 &pos2,
                  const Vector2 &pos3, const Vector4 &color,
                  kFillMode fillMode, float feather) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  if (!ctx.BindPipeline("primitive2d")) {
    return;
  }

  auto *prim = ctx.EnsurePrimitive2D();
  if (!prim) {
    return;
  }

  float thickness = 1.0f;
  prim->SetTriangle(pos1, pos2, pos3, fillMode, thickness, color, feather);
  prim->Draw(ctx.CL());
}

namespace {
/// <summary>
/// 頂点が追加された後の範囲をコマンドキューに登録するヘルパー
/// </summary>
static void AddPrimitiveCommand_(RenderContext &ctx, Primitive3D *prim,
                                 bool depth, uint32_t prevCount) {
  uint32_t currentCount = prim->GetVertexCount(depth);
  uint32_t added = currentCount - prevCount;
  if (added > 0) {
    ctx.PushPrimitive3DCommand(depth, prevCount, added);
  }
}
} // namespace

// ============================================================================
// Primitive3D
// ============================================================================

void DrawLine3D(const Vector3 &a, const Vector3 &b, const Vector4 &color,
                bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddLine(a, b, color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawAABB3D(const Vector3 &mn, const Vector3 &mx, const Vector4 &color,
                bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddAABB(mn, mx, color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawGridXZ3D(int halfSize, float step, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddGridXZ(halfSize, step, color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawGridXY3D(int halfSize, float step, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddGridXY(halfSize, step, color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawGridYZ3D(int halfSize, float step, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddGridYZ(halfSize, step, color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawWireSphere3D(const Vector3 &center, float radius,
                      const Vector4 &color, int slices, int stacks,
                      bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddSphere(center, radius, color, slices, stacks, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawSphereRings3D(const Vector3 &center, float radius,
                       const Vector4 &color, int segments, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddSphereRings(center, radius, color, segments, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawArc3D(const Vector3 &center, const Vector3 &normal,
               const Vector3 &fromDir, float radius, float startRad,
               float endRad, const Vector4 &color, int segments, bool depth,
               bool drawToCenter) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddArc(center, normal, fromDir, radius, startRad, endRad, color,
               segments, depth, drawToCenter);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawCapsule3D(const Vector3 &p0, const Vector3 &p1, float radius,
                   const Vector4 &color, int segments, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddCapsule(p0, p1, radius, color, segments, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawOBB3D(const Vector3 &center, const Vector3 &axisX,
               const Vector3 &axisY, const Vector3 &axisZ,
               const Vector3 &halfExtents, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddOBB(center, axisX, axisY, axisZ, halfExtents, color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawFrustumCorners3D(const Vector3 corners[8], const Vector4 &color,
                          bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddFrustum(corners, color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void DrawFrustum3D(const Vector3 &camPos, const Vector3 &forward,
                   const Vector3 &up, float fovYRad, float aspect, float nearZ,
                   float farZ, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  uint32_t prev = prim->GetVertexCount(depth);
  prim->AddFrustumCamera(camPos, forward, up, fovYRad, aspect, nearZ, farZ,
                         color, depth);
  AddPrimitiveCommand_(ctx, prim, depth, prev);
}

void FlushPrimitive3D() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  // コマンドキューを一括実行する（後方互換用）
  ctx.Execute3DCommands();
}


// ============================================================================
// Fog (画面オーバーレイ)
// ============================================================================

void DrawFogOverlay(float timeSec, float intensity, float scale, float speed,
                    const Vector2 &wind, float feather, float bottomBias) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL() || !ctx.Ctx() ||
      !ctx.Ctx()->pipelineManager) {
    return;
  }

  ctx.UpdateFogCB(timeSec, intensity, scale, speed, wind, feather, bottomBias);

  auto *pso = ctx.Ctx()->pipelineManager->Get(
      PipelineManager::MakeKey("fog", kBlendModeNormal));
  if (!pso) {
    return;
  }

  if (ctx.Ctx()->app) {
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(ctx.Ctx()->app->width);
    viewport.Height = static_cast<float>(ctx.Ctx()->app->height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ctx.CL()->RSSetViewports(1, &viewport);

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(ctx.Ctx()->app->width);
    scissor.bottom = static_cast<LONG>(ctx.Ctx()->app->height);
    ctx.CL()->RSSetScissorRects(1, &scissor);
  }

  ctx.CL()->SetGraphicsRootSignature(pso->Root());
  ctx.CL()->SetPipelineState(pso->PSO());
  ctx.CL()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  if (auto *fogRes = ctx.FogCBResource()) {
    ctx.CL()->SetGraphicsRootConstantBufferView(
        0, fogRes->GetGPUVirtualAddress());
  }

  ctx.CL()->DrawInstanced(3, 1, 0, 0);
}

void SetFogOverlayColor(const Vector4 &color) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  ctx.SetFogColor(color);
}

} // namespace RC
