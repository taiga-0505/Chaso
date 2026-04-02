// ============================================================================
// RenderLight.cpp
// ----------------------------------------------------------------------------
// RC::名前空間の DirectionalLight / PointLight / SpotLight / AreaLight API 実装
// ============================================================================

#include "RenderCommon.h"
#include "RenderContext.h"

#include "Light/Area/AreaLightSource.h"
#include "Light/Directional/DirectionalLightSource.h"
#include "Light/Point/PointLightSource.h"
#include "Light/Spot/SpotLightSource.h"

namespace RC {

// ============================================================================
// DirectionalLight
// ============================================================================

int CreateDirectionalLight(LightActivateMode activateMode) {
  auto &ctx = GetRenderContext();
  const int h = ctx.DirLights().Create();
  if (h < 0) {
    return h;
  }

  if (activateMode == LightActivateMode::Set) {
    ctx.DirLights().SetActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    if (!ctx.DirLights().HasExplicitActive()) {
      ctx.DirLights().SetActive(h);
    }
  }

  return h;
}

void DestroyDirectionalLight(int lightHandle) {
  GetRenderContext().DirLights().Destroy(lightHandle);
}

void SetActiveDirectionalLight(int lightHandle) {
  GetRenderContext().DirLights().SetActive(lightHandle);
}

int GetActiveDirectionalLightHandle() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return -1;
  }

  const int explicitH = ctx.DirLights().GetActiveHandle();
  if (explicitH >= 0) {
    return explicitH;
  }
  return ctx.DirLights().DefaultHandle();
}

DirectionalLightSource *GetDirectionalLightPtr(int lightHandle) {
  return GetRenderContext().DirLights().Get(lightHandle);
}

void DrawImGuiDirectionalLight(int lightHandle, const char *name) {
  auto &ctx = GetRenderContext();
  if (lightHandle < 0) {
    lightHandle = GetActiveDirectionalLightHandle();
  }
  ctx.DirLights().DrawImGui(lightHandle, name);
}

void SetDirectionalLightEnabled(int lightHandle, bool enabled) {
  auto &ctx = GetRenderContext();
  if (auto *p = ctx.DirLights().Get(lightHandle)) {
    p->SetEnabled(enabled);
    (void)ctx.DirLights().GetCBAddress(lightHandle);
  }
}

bool IsDirectionalLightEnabled(int lightHandle) {
  if (const auto *p = GetRenderContext().DirLights().Get(lightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActiveDirectionalLightEnabled(bool enabled) {
  auto &ctx = GetRenderContext();
  if (auto *p = ctx.DirLights().GetActive()) {
    p->SetEnabled(enabled);
    (void)ctx.DirLights().GetActiveCBAddress();
  }
}

bool IsActiveDirectionalLightEnabled() {
  if (const auto *p = GetRenderContext().DirLights().GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

// ============================================================================
// PointLight
// ============================================================================

int CreatePointLight(LightActivateMode activateMode) {
  auto &ctx = GetRenderContext();
  const int h = ctx.PtLights().Create();
  if (h < 0) {
    return h;
  }

  if (activateMode == LightActivateMode::Set) {
    ctx.PtLights().ClearActive();
    (void)ctx.PtLights().AddActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    (void)ctx.PtLights().AddActive(h);
  }

  return h;
}

void DestroyPointLight(int pointLightHandle) {
  GetRenderContext().PtLights().Destroy(pointLightHandle);
}

void SetActivePointLight(int pointLightHandle) {
  auto &ctx = GetRenderContext();
  ctx.PtLights().ClearActive();
  if (pointLightHandle >= 0) {
    (void)ctx.PtLights().AddActive(pointLightHandle);
  }
}

int GetActivePointLightHandle() {
  auto &ctx = GetRenderContext();
  if (ctx.PtLights().GetActiveCount() <= 0) {
    return -1;
  }
  return ctx.PtLights().GetActiveHandleAt(0);
}

void ClearActivePointLights() {
  GetRenderContext().PtLights().ClearActive();
}

bool AddActivePointLight(int pointLightHandle) {
  return GetRenderContext().PtLights().AddActive(pointLightHandle);
}

void RemoveActivePointLight(int pointLightHandle) {
  GetRenderContext().PtLights().RemoveActive(pointLightHandle);
}

int GetActivePointLightCount() {
  return GetRenderContext().PtLights().GetActiveCount();
}

int GetActivePointLightHandleAt(int index) {
  return GetRenderContext().PtLights().GetActiveHandleAt(index);
}

PointLightSource *GetPointLightPtr(int pointLightHandle) {
  return GetRenderContext().PtLights().Get(pointLightHandle);
}

void DrawImGuiPointLight(int pointLightHandle, const char *name) {
  GetRenderContext().PtLights().DrawImGui(pointLightHandle, name);
}

void SetPointLightEnabled(int pointLightHandle, bool enabled) {
  if (auto *p = GetRenderContext().PtLights().Get(pointLightHandle)) {
    p->SetEnabled(enabled);
  }
}

bool IsPointLightEnabled(int pointLightHandle) {
  if (const auto *p = GetRenderContext().PtLights().Get(pointLightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActivePointLightEnabled(bool enabled) {
  if (auto *p = GetRenderContext().PtLights().GetActive()) {
    p->SetEnabled(enabled);
  }
}

bool IsActivePointLightEnabled() {
  if (const auto *p = GetRenderContext().PtLights().GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

// ============================================================================
// SpotLight
// ============================================================================

int CreateSpotLight(LightActivateMode activateMode) {
  auto &ctx = GetRenderContext();
  const int h = ctx.SpLights().Create();
  if (h < 0) {
    return h;
  }

  if (activateMode == LightActivateMode::Set) {
    ctx.SpLights().ClearActive();
    (void)ctx.SpLights().AddActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    (void)ctx.SpLights().AddActive(h);
  }

  return h;
}

void DestroySpotLight(int spotLightHandle) {
  GetRenderContext().SpLights().Destroy(spotLightHandle);
}

void SetActiveSpotLight(int spotLightHandle) {
  auto &ctx = GetRenderContext();
  ctx.SpLights().ClearActive();
  if (spotLightHandle >= 0) {
    (void)ctx.SpLights().AddActive(spotLightHandle);
  }
}

int GetActiveSpotLightHandle() {
  auto &ctx = GetRenderContext();
  if (ctx.SpLights().GetActiveCount() <= 0) {
    return -1;
  }
  return ctx.SpLights().GetActiveHandleAt(0);
}

void ClearActiveSpotLights() {
  GetRenderContext().SpLights().ClearActive();
}

bool AddActiveSpotLight(int spotLightHandle) {
  return GetRenderContext().SpLights().AddActive(spotLightHandle);
}

void RemoveActiveSpotLight(int spotLightHandle) {
  GetRenderContext().SpLights().RemoveActive(spotLightHandle);
}

int GetActiveSpotLightCount() {
  return GetRenderContext().SpLights().GetActiveCount();
}

int GetActiveSpotLightHandleAt(int index) {
  return GetRenderContext().SpLights().GetActiveHandleAt(index);
}

SpotLightSource *GetSpotLightPtr(int spotLightHandle) {
  return GetRenderContext().SpLights().Get(spotLightHandle);
}

void DrawImGuiSpotLight(int spotLightHandle, const char *name) {
  GetRenderContext().SpLights().DrawImGui(spotLightHandle, name);
}

void SetSpotLightEnabled(int spotLightHandle, bool enabled) {
  if (auto *p = GetRenderContext().SpLights().Get(spotLightHandle)) {
    p->SetEnabled(enabled);
  }
}

bool IsSpotLightEnabled(int spotLightHandle) {
  if (const auto *p = GetRenderContext().SpLights().Get(spotLightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActiveSpotLightEnabled(bool enabled) {
  if (auto *p = GetRenderContext().SpLights().GetActive()) {
    p->SetEnabled(enabled);
  }
}

bool IsActiveSpotLightEnabled() {
  if (const auto *p = GetRenderContext().SpLights().GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

// ============================================================================
// AreaLight
// ============================================================================

int CreateAreaLight(LightActivateMode activateMode) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return -1;
  }
  const int h = ctx.ArLights().Create();
  if (h < 0) {
    return -1;
  }

  if (activateMode == LightActivateMode::Set) {
    ctx.ArLights().ClearActive();
    ctx.ArLights().AddActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    ctx.ArLights().AddActive(h);
  }
  return h;
}

void DestroyAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  ctx.ArLights().Destroy(areaLightHandle);
}

void SetActiveAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  ctx.ArLights().SetActive(areaLightHandle);
}

int GetActiveAreaLightHandle() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return -1;
  }
  return ctx.ArLights().GetActiveHandle();
}

void ClearActiveAreaLights() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  ctx.ArLights().ClearActive();
}

bool AddActiveAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return false;
  }
  return ctx.ArLights().AddActive(areaLightHandle);
}

void RemoveActiveAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  ctx.ArLights().RemoveActive(areaLightHandle);
}

int GetActiveAreaLightCount() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return 0;
  }
  return ctx.ArLights().GetActiveCount();
}

int GetActiveAreaLightHandleAt(int index) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return -1;
  }
  return ctx.ArLights().GetActiveHandleAt(index);
}

AreaLightSource *GetAreaLightPtr(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return nullptr;
  }
  return ctx.ArLights().Get(areaLightHandle);
}

void DrawImGuiAreaLight(int areaLightHandle, const char *name) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  ctx.ArLights().DrawImGui(areaLightHandle, name);
}

void SetAreaLightEnabled(int areaLightHandle, bool enabled) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  if (auto *p = ctx.ArLights().Get(areaLightHandle)) {
    p->SetEnabled(enabled);
  }
}

bool IsAreaLightEnabled(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return false;
  }
  if (const auto *p = ctx.ArLights().Get(areaLightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActiveAreaLightEnabled(bool enabled) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return;
  }
  if (auto *p = ctx.ArLights().GetActive()) {
    p->SetEnabled(enabled);
  }
}

bool IsActiveAreaLightEnabled() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return false;
  }
  if (const auto *p = ctx.ArLights().GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

} // namespace RC
