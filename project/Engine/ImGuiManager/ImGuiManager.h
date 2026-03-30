#pragma once
#include "AppConfig.h"
#include "Dx12Core.h"
#include <Windows.h>
#include <d3d12.h>

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#else
struct ImFont; // Releaseでは型だけ前方宣言
#endif

class ImGuiManager {
public:
  void
  Init(HWND hwnd, Dx12Core &core, float jpFontSize = 15.0f,
       const char *jpFontPath = "Resources/fonts/Huninn/Huninn-Regular.ttf");
  void NewFrame();
  void Render(ID3D12GraphicsCommandList *cmdList);
  void Shutdown();

  bool Enabled() const { return RC_ENABLE_IMGUI && initialized_; }

  ImFont *GetDefaultFont() const { return fontDefault_; }
  ImFont *GetJPFont() const { return fontJP_; }

private:
  bool initialized_ = false;
  ImFont *fontDefault_ = nullptr;
  ImFont *fontJP_ = nullptr;
  void reserveSrvSlotForImGui_(Dx12Core &core);
};
