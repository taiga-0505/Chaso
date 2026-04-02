// ============================================================================
// RenderSphere.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の Sphere 描画 API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "Sphere/Sphere.h"

namespace RC {

int GenerateSphereEx(int textureHandle, float radius, unsigned int sliceCount,
                     unsigned int stackCount, bool inward) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.Device()) {
    return -1;
  }

  return ctx.Spheres().Create(textureHandle, radius, sliceCount, stackCount,
                              inward);
}

int GenerateSphere(int textureHandle) {
  return GenerateSphereEx(textureHandle);
}

void DrawSphere(int sphereHandle, int texHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized() || !ctx.CL()) {
    return;
  }

  auto *s = ctx.Spheres().Get(sphereHandle);
  if (!s) {
    return;
  }

  if (!ctx.BindPipeline("object3d")) {
    return;
  }

  ctx.BindCameraCB();
  ctx.BindAllLightCBs();

  ctx.Spheres().ApplyTexture(sphereHandle, texHandle);

  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = ctx.DirLights().GetActiveCBAddress();
  s->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = ctx.DirLights().GetActive()) {
      if (!s->GetInward()) {
        if (auto *mat = s->Mat()) {
          mat->lightingMode = active->GetLightingMode();
          mat->shininess = active->GetShininess();
        }
      }
    }
  }

  s->Update(ctx.View(), ctx.Proj());
  s->Draw(ctx.CL());
}

void DrawSphereImGui(int sphereHandle, const char *name) {
  if (auto *s = GetRenderContext().Spheres().Get(sphereHandle)) {
    s->DrawImGui(name);
  }
}

void UnloadSphere(int sphereHandle) {
  GetRenderContext().Spheres().Unload(sphereHandle);
}

Transform *GetSphereTransformPtr(int sphereHandle) {
  return GetRenderContext().Spheres().GetTransformPtr(sphereHandle);
}

void SetSphereColor(int sphereHandle, const Vector4 &color) {
  GetRenderContext().Spheres().SetColor(sphereHandle, color);
}

void SetSphereLightingMode(int sphereHandle, LightingMode m) {
  GetRenderContext().Spheres().SetLightingMode(sphereHandle, m);
}

} // namespace RC
