#include "App.h"
#include "GameOverScene/GameOverScene.h"
#include "GameScene/GameScene.h"
#include "RC.h"
#include "ResultScene/ResultScene.h"
#include "SampleScene/SampleScene.h"
#include "SelectScene/SelectScene.h"
#include "TitleScene/TitleScene.h"
#include <cassert>

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
  // ====================
  // Window
  // ====================
  // ウィンドウ生成
  window_ = std::make_unique<Window>();
  window_->Initialize(appConfig_.title.c_str(), appConfig_.width,
                      appConfig_.height);

  // ====================
  // DX12 Core
  // ====================
  // コア初期化
  coreDesc_.width = appConfig_.width;
  coreDesc_.height = appConfig_.height;
  core_.Init(window_->GetHwnd(), coreDesc_);

  // デバイスとコマンドリスト取得
  cl_ = core_.CL();
  device_ = core_.GetDevice();
  assert(device_);

  // ====================
  // Input
  // ====================
  // 入力初期化
  input_ = std::make_unique<Input>(window_->GetHwnd());

  // ====================
  // ImGui
  // ====================
  // ImGui 初期化
  imgui_.Init(window_->GetHwnd(), core_);

  // ====================
  // PipelineManager
  // ====================
  // パイプライン登録
  pm_.Init(device_, coreDesc_.rtvFormat, coreDesc_.dsvFormat);

  pm_.RegisterDefaultPipelines();

  // ====================
  // SceneContext
  // ====================
  // SceneContext を紐づけ
  sceneCtx_.core = &core_;
  sceneCtx_.input = input_.get();
  sceneCtx_.app = &appConfig_;
  sceneCtx_.imgui = (RC_ENABLE_IMGUI ? &imgui_ : nullptr);
  sceneCtx_.pipelineManager = &pm_;

  // ====================
  // RenderCommon
  // ====================
  // RC 初期化
  RC::Init(sceneCtx_);

  // ====================
  // Game
  // ====================
  // ゲーム初期化
  game_.Init(sceneCtx_);

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
      Render();
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
  // Pipeline / ImGui
  // ====================
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
