#include "ImGuiManager.h"
#include <cassert>

void ImGuiManager::reserveSrvSlotForImGui_(Dx12Core &core) {
  core.SRV().AllocateCPU(
      1); // スロット0を確保（以降は1〜） :contentReference[oaicite:5]{index=5}
}

void ImGuiManager::Init(HWND hwnd, Dx12Core &core, float jpFontSize,
                        const char *jpFontPath) {
  if (initialized_)
    return;

#if RC_ENABLE_IMGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiIO &io = ImGui::GetIO();
  fontDefault_ = io.FontDefault;
  fontJP_ = io.Fonts->AddFontFromFileTTF(jpFontPath, jpFontSize, nullptr,
                                         io.Fonts->GetGlyphRangesJapanese());
  if (fontJP_)
    io.FontDefault = fontJP_;

  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(core.GetDevice(), core.FrameCount(),
                      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, core.SRV().Heap(),
                      core.SRV().CPUAt(0), core.SRV().GPUAt(0));
#endif

  // ImGui有無に関わらずスロット0は予約して並びを固定
  reserveSrvSlotForImGui_(core);

  initialized_ = true;
}

void ImGuiManager::NewFrame() {
#if RC_ENABLE_IMGUI
  if (!initialized_)
    return;
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
#endif
}

void ImGuiManager::Render(ID3D12GraphicsCommandList *cmdList) {
#if RC_ENABLE_IMGUI
  if (!initialized_)
    return;
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
#endif
}

void ImGuiManager::Shutdown() {
#if RC_ENABLE_IMGUI
  if (!initialized_)
    return;
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
#endif
  initialized_ = false;
}
