#include "App.h"
#include "GameOverScene/GameOverScene.h"
#include "GameScene/GameScene.h"
#include "RC.h"
#include "ResultScene/ResultScene.h"
#include "SampleScene/SampleScene.h"
#include "SelectScene/SelectScene.h"
#include "TitleScene/TitleScene.h"
#include "SceneManager.h"
#include <cassert>
#include <chrono>
#include <format>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#include <dxgidebug.h>
#include <wrl.h>
inline void ReportLiveObjectsDbg(const char *tag) {
  using Microsoft::WRL::ComPtr;
  OutputDebugStringA(
      ("==== LIVE REPORT @" + std::string(tag) + " ====\n").c_str());
      
  // Report DXGI
  ComPtr<IDXGIDebug> dxgiDbg;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDbg)))) {
    dxgiDbg->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
  }
}

App::App() {}
App::~App() {}

bool App::Init() {
  auto totalStart = std::chrono::high_resolution_clock::now();
  Log::Print("[App] 初期化開始...");

  auto stepStart = std::chrono::high_resolution_clock::now();

  // Window
  window_ = std::make_unique<Window>();
  window_->Initialize(appConfig_.title.c_str(), appConfig_.width, appConfig_.height);
  
  auto now = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] Window 生成完了 (Time: {:.3f}ms)", std::chrono::duration<float, std::milli>(now - stepStart).count()));
  stepStart = now;

  // DX12 Core
  coreDesc_.width = appConfig_.width;
  coreDesc_.height = appConfig_.height;
  core_.Init(window_->GetHwnd(), coreDesc_);
  cl_ = core_.CL();
  device_ = core_.GetDevice();
  assert(device_);

  now = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] DirectX12 Core 初期化完了 (Time: {:.3f}ms)", std::chrono::duration<float, std::milli>(now - stepStart).count()));
  stepStart = now;

  // Input
  input_ = std::make_unique<Input>(window_->GetHwnd());

  now = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] Input 初期化完了 (Time: {:.3f}ms)", std::chrono::duration<float, std::milli>(now - stepStart).count()));
  stepStart = now;

  // ImGui
  imgui_.Init(window_->GetHwnd(), core_);

  now = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] ImGui 初期化完了 (Time: {:.3f}ms)", std::chrono::duration<float, std::milli>(now - stepStart).count()));
  stepStart = now;

  // PipelineManager
  pm_.Init(device_, coreDesc_.rtvFormat, coreDesc_.dsvFormat);
  pm_.RegisterDefaultPipelines();

  now = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] PipelineManager 初期化完了 (Time: {:.3f}ms)", std::chrono::duration<float, std::milli>(now - stepStart).count()));
  stepStart = now;

  // PostProcess
  renderTexture_.Initialize(&core_, appConfig_.width, appConfig_.height);
  postProcess_ = std::make_unique<PostProcess>();
  postProcess_->Initialize(&core_, &pm_, appConfig_.width, appConfig_.height);

  now = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] PostProcess 初期化完了 (Time: {:.3f}ms)", std::chrono::duration<float, std::milli>(now - stepStart).count()));
  stepStart = now;

  // SceneContext / RC
  sceneCtx_.core = &core_;
  sceneCtx_.input = input_.get();
  sceneCtx_.app = &appConfig_;
  sceneCtx_.imgui = (RC_ENABLE_IMGUI ? &imgui_ : nullptr);
  sceneCtx_.pipelineManager = &pm_;
  sceneCtx_.postProcess = postProcess_.get();
  RC::Init(sceneCtx_);

  now = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] RenderContext 初期化完了 (Time: {:.3f}ms)", std::chrono::duration<float, std::milli>(now - stepStart).count()));
  stepStart = now;

  // Game (Initial Scene Load)
  game_.Init(sceneCtx_);

  auto totalEnd = std::chrono::high_resolution_clock::now();
  Log::Print(std::format("[App] 初期化完了 (Total Time: {:.3f}s)", std::chrono::duration<float>(totalEnd - totalStart).count()));

  return true;
}

int App::Run() {
  // ====================
  // Main Loop
  // ====================
  // メッセージループ
  while (msg_.message != WM_QUIT) {
    if (PeekMessage(&msg_, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg_);
      DispatchMessage(&msg_);
    } else {
#if RC_ENABLE_IMGUI
      // ImGui フレーム開始
      imgui_.NewFrame();
#endif
      // 更新
      Update();

      // 描画
      core_.BeginFrame();

      // --- オフスクリーンレンダリング開始 ---
      renderTexture_.TransitionToRenderTarget(cl_);

      // RenderTextureをクリアしてセット（青）
      float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
      cl_->ClearRenderTargetView(renderTexture_.GetRTV(), clearColor, 0, nullptr);
      
      // 描画先をRenderTextureに、深度バッファはそのまま
      D3D12_CPU_DESCRIPTOR_HANDLE rtv = renderTexture_.GetRTV();
      D3D12_CPU_DESCRIPTOR_HANDLE dsv = core_.Dsv();
      cl_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

      Render();

      // --- オフスクリーンレンダリング終了 ---
      renderTexture_.TransitionToShaderResource(cl_);

      // 描画先をバックバッファ（画面）に戻す
      D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv = core_.CurrentRTV();
      cl_->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);

      // ビューポートとシザー矩形をバックバッファサイズに再設定（必須）
      core_.ResetViewportScissorToBackbuffer(appConfig_.width, appConfig_.height);

      // ポストプロセス（RenderTextureの内容を画面に転送）
      postProcess_->Draw(cl_, renderTexture_);

#if RC_ENABLE_IMGUI
      // ImGui 描画
      imgui_.Render(cl_);
#endif
      core_.EndFrame();
    }
  }
  return static_cast<int>(msg_.wParam);
}

void App::Update() {

#if RC_ENABLE_IMGUI
  // ====================
  // Debug UI
  // ====================
  // デバッグUI描画
  game_.DrawDebugUI();
#endif

  // ====================
  // Input
  // ====================
  // 入力更新
  input_->Update();

  // ====================
  // Game
  // ====================
  // ゲーム更新
  game_.Update(sceneCtx_);
}

void App::Render() {
  // ====================
  // Game Render
  // ====================
  // ゲーム描画
  game_.Render(sceneCtx_, cl_);
}

void App::Term() {
  // ====================
  // GPU Wait
  // ====================
  // GPU 完了待ち
  core_.WaitForGPU();

  // ====================
  // Game
  // ====================
  // ゲーム終了
  game_.Term();

  // ====================
  // Render Layer
  // ====================
  // RenderCommon 終了
  RC::Term();

  // ====================
  // Pipeline / ImGui / RenderObjects
  // ====================
  // 追加リソースを解放
  renderTexture_.Release();
  postProcess_.reset();

  // パイプラインと ImGui 終了
  pm_.Term();
  imgui_.Shutdown();

  // ====================
  // Core
  // ====================
  // コア終了
  core_.Term();
  device_ = nullptr;
  cl_ = nullptr;

  // ====================
  // Input / Window
  // ====================
  // 入力とウィンドウ破棄
  input_.reset();
  window_.reset();

  ReportLiveObjectsDbg("FINAL Report at the end of App::Term");
}
