// ============================================================================
// RenderModel.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の Model 描画 API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "Model3d/ModelObject.h"

namespace RC {

// ============================================================================
// ライト適用の共通ヘルパー（file-scope）
// ============================================================================
namespace {

/// ディレクショナルライトの CB アドレスをモデルに適用し、
/// ライティングモード/shininess をマテリアルに同期する。
/// @return lightAddr (0 = デフォルト使用)
static D3D12_GPU_VIRTUAL_ADDRESS ApplyDirLight_(RenderContext &ctx,
                                                ModelObject *m) {
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      ctx.DirLights().GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = ctx.DirLights().GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }
  }
  return lightAddr;
}

/// ガラス用ライト適用（shininess は上書きしない）
static void ApplyDirLightGlass_(RenderContext &ctx, ModelObject *m) {
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      ctx.DirLights().GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = ctx.DirLights().GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
      }
    }
  }
}

/// 標準的な 3D バインド（Pipeline + Camera + Lights）
static bool BindStandard3D_(RenderContext &ctx, std::string_view prefix) {
  if (!ctx.BindPipeline(prefix)) {
    return false;
  }
  ctx.BindCameraCB();
  ctx.BindAllLightCBs();
  return true;
}

/// テクスチャのオーバーライド
static void ApplyTexture_(RenderContext &ctx, ModelObject *m, int texHandle) {
  if (texHandle >= 0) {
    m->SetTexture(ctx.Textures().GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }
}

} // namespace

// ============================================================================
// Load / Unload
// ============================================================================

int LoadModel(const std::string &path) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return -1;
  }
  return ctx.Models().Load(path);
}

void UnloadModel(int modelHandle) {
  GetRenderContext().Models().Unload(modelHandle);
}

// ============================================================================
// Draw (基本)
// ============================================================================

void DrawModel(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindStandard3D_(ctx, "object3d")) {
    return;
  }

  ApplyTexture_(ctx, m, texHandle);
  const auto lightAddr = ApplyDirLight_(ctx, m);

  m->Update(ctx.View(), ctx.Proj());
  m->Draw(ctx.CL());

  if (lightAddr == 0) {
    m->SetExternalLightCBAddress(0);
  }
}

void DrawModel(int modelHandle) { DrawModel(modelHandle, -1); }

void DrawModelNoCull(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindStandard3D_(ctx, "object3d_nocull")) {
    return;
  }

  ApplyTexture_(ctx, m, texHandle);
  const auto lightAddr = ApplyDirLight_(ctx, m);

  m->Update(ctx.View(), ctx.Proj());
  m->Draw(ctx.CL());

  if (lightAddr == 0) {
    m->SetExternalLightCBAddress(0);
  }
}

// ============================================================================
// Batch
// ============================================================================

void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindStandard3D_(ctx, "object3d_inst")) {
    return;
  }

  ApplyTexture_(ctx, m, texHandle);
  const auto lightAddr = ApplyDirLight_(ctx, m);

  m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances);

  if (lightAddr == 0) {
    m->SetExternalLightCBAddress(0);
  }
}

void DrawModelBatchColored(int modelHandle,
                           const std::vector<Transform> &instances,
                           const Vector4 &color, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindStandard3D_(ctx, "object3d_inst")) {
    return;
  }

  ApplyTexture_(ctx, m, texHandle);
  const auto lightAddr = ApplyDirLight_(ctx, m);

  m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances, color);

  if (lightAddr == 0) {
    m->SetExternalLightCBAddress(0);
  }
}

// ============================================================================
// Glass (1パス)
// ============================================================================

void DrawModelGlass(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModePremultiplied);

  if (!BindStandard3D_(ctx, "object3d_glass")) {
    ctx.SetBlendMode(saved);
    return;
  }

  ApplyTexture_(ctx, m, texHandle);
  ApplyDirLightGlass_(ctx, m);

  m->Update(ctx.View(), ctx.Proj());
  m->Draw(ctx.CL());

  ctx.SetBlendMode(saved);
}

void DrawModelGlassBatch(int modelHandle,
                         const std::vector<Transform> &instances,
                         int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModePremultiplied);

  if (!BindStandard3D_(ctx, "object3d_glass_inst")) {
    ctx.SetBlendMode(saved);
    return;
  }

  ApplyTexture_(ctx, m, texHandle);
  ApplyDirLightGlass_(ctx, m);

  m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances);

  ctx.SetBlendMode(saved);
}

void DrawModelGlassBatchColored(int modelHandle,
                                const std::vector<Transform> &instances,
                                const Vector4 &color, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModePremultiplied);

  if (!BindStandard3D_(ctx, "object3d_glass_inst")) {
    ctx.SetBlendMode(saved);
    return;
  }

  ApplyTexture_(ctx, m, texHandle);
  ApplyDirLightGlass_(ctx, m);

  m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances, color);

  ctx.SetBlendMode(saved);
}

// ============================================================================
// Glass (2パス: 背面→表面)
// ============================================================================

void DrawModelGlassTwoPass(int modelHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModePremultiplied);

  ApplyTexture_(ctx, m, texHandle);
  ApplyDirLightGlass_(ctx, m);
  m->Update(ctx.View(), ctx.Proj());

  // 1) 背面（内側）
  if (BindStandard3D_(ctx, "object3d_glass_front")) {
    m->Draw(ctx.CL());
  }
  // 2) 表面（外側）
  if (BindStandard3D_(ctx, "object3d_glass")) {
    m->Draw(ctx.CL());
  }

  ctx.SetBlendMode(saved);
}

void DrawModelGlassTwoPassBatch(int modelHandle,
                                const std::vector<Transform> &instances,
                                int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModePremultiplied);

  ApplyTexture_(ctx, m, texHandle);
  ApplyDirLightGlass_(ctx, m);
  m->Update(ctx.View(), ctx.Proj());

  if (BindStandard3D_(ctx, "object3d_glass_front_inst")) {
    m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances);
  }
  if (BindStandard3D_(ctx, "object3d_glass_inst")) {
    m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances);
  }

  ctx.SetBlendMode(saved);
}

void DrawModelGlassTwoPassBatchColored(int modelHandle,
                                       const std::vector<Transform> &instances,
                                       const Vector4 &color, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = ctx.CurrentBlendMode();
  ctx.SetBlendMode(kBlendModePremultiplied);

  ApplyTexture_(ctx, m, texHandle);
  ApplyDirLightGlass_(ctx, m);
  m->Update(ctx.View(), ctx.Proj());

  if (BindStandard3D_(ctx, "object3d_glass_front_inst")) {
    m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances, color);
  }
  if (BindStandard3D_(ctx, "object3d_glass_inst")) {
    m->DrawBatch(ctx.CL(), ctx.View(), ctx.Proj(), instances, color);
  }

  ctx.SetBlendMode(saved);
}

// ============================================================================
// ImGui / Transform / Color / LightingMode / Mesh / Cursor
// ============================================================================

void DrawImGui3D(int modelHandle, const char *name) {
  auto &ctx = GetRenderContext();
  auto *m = ctx.Models().Get(modelHandle);
  if (!m) {
    return;
  }

  const bool showLightingUi = (ctx.DirLights().GetActiveCBAddress() == 0);
  m->DrawImGui(name, showLightingUi);
}

Transform *GetModelTransformPtr(int modelHandle) {
  return GetRenderContext().Models().GetTransformPtr(modelHandle);
}

void SetModelColor(int modelHandle, const Vector4 &color) {
  GetRenderContext().Models().SetColor(modelHandle, color);
}

void SetModelLightingMode(int modelHandle, LightingMode m) {
  GetRenderContext().Models().SetLightingMode(modelHandle, m);
}

void SetModelMesh(int modelHandle, const std::string &path) {
  GetRenderContext().Models().SetMesh(modelHandle, path);
}

void ResetCursor(int modelHandle) {
  GetRenderContext().Models().ResetCursor(modelHandle);
}

} // namespace RC
