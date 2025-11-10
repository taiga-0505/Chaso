#include "App.h"
#include "GameOverScene/GameOverScene.h"
#include "GameScene/GameScene.h"
#include "RC.h"
#include "ResultScene/ResultScene.h"
#include "SampleScene/SampleScene.h"
#include "SelectScene/SelectScene.h"
#include "TitleScene/TitleScene.h"
#include "imgui/imgui.h"
#include <cassert>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#include <dxgidebug.h>
#include <wrl.h>
inline void ReportDXGI(const char *tag) {
  using Microsoft::WRL::ComPtr;
  ComPtr<IDXGIDebug> dbg;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dbg)))) {
    OutputDebugStringA(
        ("==== DXGI LIVE REPORT @" + std::string(tag) + " ====\n").c_str());
    dbg->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
  }
}

App::App() {}
App::~App() {}

bool App::Init() {
  // ===== Window =====
  window_ = std::make_unique<Window>();
  window_->Initialize(appConfig_.title.c_str(), appConfig_.width,
                      appConfig_.height);

  // ===== DX12 Core =====
  coreDesc_.width = appConfig_.width;
  coreDesc_.height = appConfig_.height;
  core_.Init(window_->GetHwnd(), coreDesc_);

  cl = core_.CL();
  device = core_.GetDevice();
  assert(device);

  // ===== Input =====
  input_ = std::make_unique<Input>(window_->GetHwnd());

  // ===== ImGui =====
  imgui_.Init(window_->GetHwnd(), core_);

  // ===== PipelineManager =====
  pm_.Init(device, coreDesc_.rtvFormat, coreDesc_.dsvFormat);

  // ===========================================
  // ModelPSO ブレンド違いをキー名で登録
  // ===========================================

  // GraphicsPipeline一括構築（既存のObject3D）
  pipeObj_ = pm_.CreateFromFiles("object3d", L"Shader/Object3D.VS.hlsl",
                                 L"Shader/Object3D.PS.hlsl",
                                 InputLayoutType::Object3D);

  pm_.CreateModelPipeline("ObjBlendModeNone", L"Shader/Object3D.VS.hlsl",
                          L"Shader/Object3D.PS.hlsl", kBlendModeNone);

  pm_.CreateModelPipeline("ObjBlendModeNormal", L"Shader/Object3D.VS.hlsl",
                          L"Shader/Object3D.PS.hlsl", kBlendModeNormal);

  pm_.CreateModelPipeline("ObjBlendModeAdd", L"Shader/Object3D.VS.hlsl",
                          L"Shader/Object3D.PS.hlsl", kBlendModeAdd);

  pm_.CreateModelPipeline("ObjBlendModeSubtract", L"Shader/Object3D.VS.hlsl",
                          L"Shader/Object3D.PS.hlsl", kBlendModeSubtract);

  pm_.CreateModelPipeline("ObjBlendModeMultiply", L"Shader/Object3D.VS.hlsl",
                          L"Shader/Object3D.PS.hlsl", kBlendModeMultiply);

  pm_.CreateModelPipeline("ObjBlendModeScreen", L"Shader/Object3D.VS.hlsl",
                          L"Shader/Object3D.PS.hlsl", kBlendModeScreen);

  // デフォルト（Normal）を SceneContext へ（objectPSO）
  sceneCtx_.objectPSO = pm_.Get("ObjBlendModeNone");

  // ===========================================
  // SpritePSO ブレンド違いをキー名で登録
  // ===========================================

  /*pipeSprite_ = pm_.CreateFromFiles("sprite", L"Shader/Sprite.VS.hlsl",
                                 L"Shader/Sprite.PS.hlsl",
     InputLayoutType::Sprite);*/

  pm_.CreateSpritePipeline("BlendModeNone", L"Shader/Object3D.VS.hlsl",
                           L"Shader/Object3D.PS.hlsl", kBlendModeNone);

  pm_.CreateSpritePipeline("BlendModeNormal", L"Shader/Object3D.VS.hlsl",
                           L"Shader/Object3D.PS.hlsl", kBlendModeNormal);

  pm_.CreateSpritePipeline("BlendModeAdd", L"Shader/Object3D.VS.hlsl",
                           L"Shader/Object3D.PS.hlsl", kBlendModeAdd);

  pm_.CreateSpritePipeline("BlendModeSubtract", L"Shader/Object3D.VS.hlsl",
                           L"Shader/Object3D.PS.hlsl", kBlendModeSubtract);

  pm_.CreateSpritePipeline("BlendModeMultiply", L"Shader/Object3D.VS.hlsl",
                           L"Shader/Object3D.PS.hlsl", kBlendModeMultiply);

  pm_.CreateSpritePipeline("BlendModeScreen", L"Shader/Object3D.VS.hlsl",
                           L"Shader/Object3D.PS.hlsl", kBlendModeScreen);

  // デフォルト（Normal）を SceneContext へ
  pipeSprite_ = pm_.Get("BlendModeNormal");

  // ===== SceneContext の紐づけ =====
  sceneCtx_.core = &core_;
  sceneCtx_.input = input_.get();
  sceneCtx_.app = &appConfig_;
  sceneCtx_.imgui = &imgui_;
  sceneCtx_.objectPSO = pipeObj_;
  sceneCtx_.spritePSO = pipeSprite_;
  sceneCtx_.pipelineManager = &pm_;

  // ===== RC初期化 =====
  RC::Init(sceneCtx_);

  // ===== Game初期化 =====
  game_.Init(sceneCtx_);

  return true;
}

int App::Run() {
  // メッセージループ
  while (msg_.message != WM_QUIT) {
    if (PeekMessage(&msg_, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg_);
      DispatchMessage(&msg_);
    } else {

      imgui_.NewFrame();
      Update();
      core_.BeginFrame();
      Render();
      imgui_.Render(cl);
      core_.EndFrame();
    }
  }
  return static_cast<int>(msg_.wParam);
}

void App::Update() {

  game_.DrawDebugUI();

  input_->Update();

  game_.Update(sceneCtx_);
}

void App::Render() { game_.Render(sceneCtx_, cl); }

void App::Term() {
  core_.WaitForGPU();

  game_.Term();

  // 3) 描画層
  ReportDXGI("before RC::Term");
  RC::Term();
  ReportDXGI("after RC::Term");

  pm_.Term();
  imgui_.Shutdown();

  // 4) コア（SwapChain/デバイスなど最終）
  ReportDXGI("before core_.Term");
  core_.Term();
  ReportDXGI("after core_.Term");

  // 5) 入力/ウィンドウ
  input_.reset();
  window_.reset();
}
