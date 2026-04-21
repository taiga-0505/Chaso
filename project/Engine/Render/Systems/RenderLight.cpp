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

#include "Common/Log/Log.h"
#include <format>

#include <concepts>
#include <type_traits>

namespace RC {

namespace {

// ============================================================================
// テンプレートヘルパーと Concept
// ============================================================================

template <typename T>
concept LightManager = requires(T & m, int h) {
  { m.Create() } -> std::same_as<int>;
  { m.Destroy(h) };
  { m.Get(h) };
  { m.DrawImGui(h, (const char *)nullptr) };
  { m.GetActiveHandle() } -> std::same_as<int>;
  { m.SetActive(h) };
};

template <typename T>
concept MultiActiveManager = LightManager<T> && requires(T & m, int h) {
  { m.AddActive(h) } -> std::same_as<bool>;
  { m.ClearActive() };
  { m.RemoveActive(h) };
  { m.GetActiveCount() } -> std::same_as<int>;
  { m.GetActiveHandleAt(0) } -> std::same_as<int>;
};

template <LightManager T> int TCreateLight(T &mgr, LightActivateMode activateMode) {
  const int h = mgr.Create();
  if (h < 0) {
    return h;
  }

  if (activateMode == LightActivateMode::Set) {
    if constexpr (MultiActiveManager<T>) {
      mgr.ClearActive();
      (void)mgr.AddActive(h);
    } else {
      mgr.SetActive(h);
    }
  } else if (activateMode == LightActivateMode::Add) {
    if constexpr (MultiActiveManager<T>) {
      (void)mgr.AddActive(h);
    } else {
      // Directional 等、単一アクティブのみをサポートする場合
      if constexpr (requires { mgr.HasExplicitActive(); }) {
        if (!mgr.HasExplicitActive()) {
          mgr.SetActive(h);
        }
      } else {
        if (mgr.GetActiveHandle() < 0) {
          mgr.SetActive(h);
        }
      }
    }
  }
  return h;
}

template <LightManager T> void TDestroyLight(T &mgr, int handle) {
  mgr.Destroy(handle);
}

template <LightManager T> void TSetActiveLight(T &mgr, int handle) {
  if constexpr (MultiActiveManager<T>) {
    mgr.ClearActive();
    if (handle >= 0) {
      (void)mgr.AddActive(handle);
    }
  } else {
    mgr.SetActive(handle);
  }
}

template <LightManager T> int TGetActiveLightHandle(const T &mgr) {
  const int h = mgr.GetActiveHandle();
  if constexpr (!MultiActiveManager<T> && requires { mgr.DefaultHandle(); }) {
    if (h >= 0)
      return h;
    return mgr.DefaultHandle();
  }
  return h;
}

template <LightManager T> void TDrawImGuiLight(T &mgr, int handle, const char *name) {
  if (handle < 0) {
    handle = TGetActiveLightHandle(mgr);
  }
  mgr.DrawImGui(handle, name);
}

template <LightManager T> void TSetLightEnabled(T &mgr, int handle, bool enabled) {
  if (auto *p = mgr.Get(handle)) {
    p->SetEnabled(enabled);
    if constexpr (requires { mgr.GetCBAddress(handle); }) {
       (void)mgr.GetCBAddress(handle);
    }
  }
}

template <LightManager T> bool TIsLightEnabled(const T &mgr, int handle) {
  if (const auto *p = mgr.Get(handle)) {
    return p->IsEnabled();
  }
  return false;
}

template <LightManager T> void TSetActiveLightEnabled(T &mgr, bool enabled) {
  if (auto *p = mgr.GetActive()) {
    p->SetEnabled(enabled);
    if constexpr (requires { mgr.GetActiveCBAddress(); }) {
      (void)mgr.GetActiveCBAddress();
    }
  }
}

template <LightManager T> bool TIsActiveLightEnabled(const T &mgr) {
  if (const auto *p = mgr.GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

} // namespace

// ============================================================================
// DirectionalLight
// ============================================================================

int CreateDirectionalLight(LightActivateMode activateMode) {
  int h = TCreateLight(GetRenderContext().DirLights(), activateMode);
  if (h >= 0) Log::Print(std::format("[Light] DirectionalLight 生成 (Handle: {})", h));
  return h;
}

void DestroyDirectionalLight(int lightHandle) {
  TDestroyLight(GetRenderContext().DirLights(), lightHandle);
  Log::Print(std::format("[Light] DirectionalLight 破棄完了 (Handle: {})", lightHandle));
}

void SetActiveDirectionalLight(int lightHandle) {
  TSetActiveLight(GetRenderContext().DirLights(), lightHandle);
}

int GetActiveDirectionalLightHandle() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) {
    return -1;
  }
  return TGetActiveLightHandle(ctx.DirLights());
}

DirectionalLightSource *GetDirectionalLightPtr(int lightHandle) {
  return GetRenderContext().DirLights().Get(lightHandle);
}

void DrawImGuiDirectionalLight(int lightHandle, const char *name) {
  TDrawImGuiLight(GetRenderContext().DirLights(), lightHandle, name);
}

void SetDirectionalLightEnabled(int lightHandle, bool enabled) {
  TSetLightEnabled(GetRenderContext().DirLights(), lightHandle, enabled);
}

bool IsDirectionalLightEnabled(int lightHandle) {
  return TIsLightEnabled(GetRenderContext().DirLights(), lightHandle);
}

void SetActiveDirectionalLightEnabled(bool enabled) {
  TSetActiveLightEnabled(GetRenderContext().DirLights(), enabled);
}

bool IsActiveDirectionalLightEnabled() {
  return TIsActiveLightEnabled(GetRenderContext().DirLights());
}

// ============================================================================
// PointLight
// ============================================================================

int CreatePointLight(LightActivateMode activateMode) {
  int h = TCreateLight(GetRenderContext().PtLights(), activateMode);
  if (h >= 0) Log::Print(std::format("[Light] PointLight 生成 (Handle: {})", h));
  return h;
}

void DestroyPointLight(int pointLightHandle) {
  TDestroyLight(GetRenderContext().PtLights(), pointLightHandle);
  Log::Print(std::format("[Light] PointLight 破棄完了 (Handle: {})", pointLightHandle));
}

void SetActivePointLight(int pointLightHandle) {
  TSetActiveLight(GetRenderContext().PtLights(), pointLightHandle);
}

int GetActivePointLightHandle() {
  return TGetActiveLightHandle(GetRenderContext().PtLights());
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
  TDrawImGuiLight(GetRenderContext().PtLights(), pointLightHandle, name);
}

void SetPointLightEnabled(int pointLightHandle, bool enabled) {
  TSetLightEnabled(GetRenderContext().PtLights(), pointLightHandle, enabled);
}

bool IsPointLightEnabled(int pointLightHandle) {
  return TIsLightEnabled(GetRenderContext().PtLights(), pointLightHandle);
}

void SetActivePointLightEnabled(bool enabled) {
  TSetActiveLightEnabled(GetRenderContext().PtLights(), enabled);
}

bool IsActivePointLightEnabled() {
  return TIsActiveLightEnabled(GetRenderContext().PtLights());
}

// ============================================================================
// SpotLight
// ============================================================================

int CreateSpotLight(LightActivateMode activateMode) {
  int h = TCreateLight(GetRenderContext().SpLights(), activateMode);
  if (h >= 0) Log::Print(std::format("[Light] SpotLight 生成 (Handle: {})", h));
  return h;
}

void DestroySpotLight(int spotLightHandle) {
  TDestroyLight(GetRenderContext().SpLights(), spotLightHandle);
  Log::Print(std::format("[Light] SpotLight 破棄完了 (Handle: {})", spotLightHandle));
}

void SetActiveSpotLight(int spotLightHandle) {
  TSetActiveLight(GetRenderContext().SpLights(), spotLightHandle);
}

int GetActiveSpotLightHandle() {
  return TGetActiveLightHandle(GetRenderContext().SpLights());
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
  TDrawImGuiLight(GetRenderContext().SpLights(), spotLightHandle, name);
}

void SetSpotLightEnabled(int spotLightHandle, bool enabled) {
  TSetLightEnabled(GetRenderContext().SpLights(), spotLightHandle, enabled);
}

bool IsSpotLightEnabled(int spotLightHandle) {
  return TIsLightEnabled(GetRenderContext().SpLights(), spotLightHandle);
}

void SetActiveSpotLightEnabled(bool enabled) {
  TSetActiveLightEnabled(GetRenderContext().SpLights(), enabled);
}

bool IsActiveSpotLightEnabled() {
  return TIsActiveLightEnabled(GetRenderContext().SpLights());
}

// ============================================================================
// AreaLight
// ============================================================================

int CreateAreaLight(LightActivateMode activateMode) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return -1;
  int h = TCreateLight(ctx.ArLights(), activateMode);
  if (h >= 0) Log::Print(std::format("[Light] AreaLight 生成 (Handle: {})", h));
  return h;
}

void DestroyAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  TDestroyLight(ctx.ArLights(), areaLightHandle);
  Log::Print(std::format("[Light] AreaLight 破棄完了 (Handle: {})", areaLightHandle));
}

void SetActiveAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  TSetActiveLight(ctx.ArLights(), areaLightHandle);
}

int GetActiveAreaLightHandle() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return -1;
  return TGetActiveLightHandle(ctx.ArLights());
}

void ClearActiveAreaLights() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  ctx.ArLights().ClearActive();
}

bool AddActiveAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return false;
  return ctx.ArLights().AddActive(areaLightHandle);
}

void RemoveActiveAreaLight(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  ctx.ArLights().RemoveActive(areaLightHandle);
}

int GetActiveAreaLightCount() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return 0;
  return ctx.ArLights().GetActiveCount();
}

int GetActiveAreaLightHandleAt(int index) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return -1;
  return ctx.ArLights().GetActiveHandleAt(index);
}

AreaLightSource *GetAreaLightPtr(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return nullptr;
  return ctx.ArLights().Get(areaLightHandle);
}

void DrawImGuiAreaLight(int areaLightHandle, const char *name) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  TDrawImGuiLight(ctx.ArLights(), areaLightHandle, name);
}

void SetAreaLightEnabled(int areaLightHandle, bool enabled) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  TSetLightEnabled(ctx.ArLights(), areaLightHandle, enabled);
}

bool IsAreaLightEnabled(int areaLightHandle) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return false;
  return TIsLightEnabled(ctx.ArLights(), areaLightHandle);
}

void SetActiveAreaLightEnabled(bool enabled) {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return;
  TSetActiveLightEnabled(ctx.ArLights(), enabled);
}

bool IsActiveAreaLightEnabled() {
  auto &ctx = GetRenderContext();
  if (!ctx.IsInitialized()) return false;
  return TIsActiveLightEnabled(ctx.ArLights());
}

} // namespace RC
