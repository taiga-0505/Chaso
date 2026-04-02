// ============================================================================
// RenderPrimitive.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の Primitive2D / Primitive3D / Fog API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "PipelineManager.h"
#include "Primitive2D/Primitive2D.h"
#include "Primitive3D/Primitive3D.h"
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

// ============================================================================
// Primitive3D
// ============================================================================

void DrawLine3D(const Vector3 &a, const Vector3 &b, const Vector4 &color,
                bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddLine(a, b, color, depth);
}

void DrawAABB3D(const Vector3 &mn, const Vector3 &mx, const Vector4 &color,
                bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddAABB(mn, mx, color, depth);
}

void DrawGridXZ3D(int halfSize, float step, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddGridXZ(halfSize, step, color, depth);
}

void DrawGridXY3D(int halfSize, float step, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddGridXY(halfSize, step, color, depth);
}

void DrawGridYZ3D(int halfSize, float step, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddGridYZ(halfSize, step, color, depth);
}

void DrawWireSphere3D(const Vector3 &center, float radius,
                      const Vector4 &color, int slices, int stacks,
                      bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddSphere(center, radius, color, slices, stacks, depth);
}

void DrawSphereRings3D(const Vector3 &center, float radius,
                       const Vector4 &color, int segments, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddSphereRings(center, radius, color, segments, depth);
}

void DrawArc3D(const Vector3 &center, const Vector3 &normal,
               const Vector3 &fromDir, float radius, float startRad,
               float endRad, const Vector4 &color, int segments, bool depth,
               bool drawToCenter) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddArc(center, normal, fromDir, radius, startRad, endRad, color,
               segments, depth, drawToCenter);
}

void DrawCapsule3D(const Vector3 &p0, const Vector3 &p1, float radius,
                   const Vector4 &color, int segments, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddCapsule(p0, p1, radius, color, segments, depth);
}

void DrawOBB3D(const Vector3 &center, const Vector3 &axisX,
               const Vector3 &axisY, const Vector3 &axisZ,
               const Vector3 &halfExtents, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddOBB(center, axisX, axisY, axisZ, halfExtents, color, depth);
}

void DrawFrustumCorners3D(const Vector3 corners[8], const Vector4 &color,
                          bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddFrustum(corners, color, depth);
}

void DrawFrustum3D(const Vector3 &camPos, const Vector3 &forward,
                   const Vector3 &up, float fovYRad, float aspect, float nearZ,
                   float farZ, const Vector4 &color, bool depth) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }
  prim->AddFrustumCamera(camPos, forward, up, fovYRad, aspect, nearZ, farZ,
                         color, depth);
}

void FlushPrimitive3D() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *prim = ctx.EnsurePrimitive3D();
  if (!prim) {
    return;
  }

  if (!prim->HasAny()) {
    return;
  }

  const BlendMode saved = ctx.CurrentBlendMode();
  ctx.SetBlendMode(prim->BlendAt3D());

  if (ctx.BindPipeline("primitive3d")) {
    ctx.CL()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    prim->Draw(ctx.CL(), true);
  }
  if (ctx.BindPipeline("primitive3d_nodepth")) {
    ctx.CL()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    prim->Draw(ctx.CL(), false);
  }

  ctx.SetBlendMode(saved);
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
