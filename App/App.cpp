#include "App.h"
#include "GameScene/GameScene.h"
#include "RenderCommon.h"
#include "ResultScene/ResultScene.h"
#include "SampleScene/SampleScene.h"
#include "SelectScene/SelectScene.h"
#include "TitleScene/TitleScene.h"
#include "imgui/imgui.h"
#include <cassert>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

Log logger;

App::App() {}
App::~App() { Term(); }

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

  RC::Init(sceneCtx_);

  // ===== FadeのためのScene初期化 =====
  sceneMgr_.Init(sceneCtx_); // appが必要なため、ここで初期化

  // ===== シーン登録 =====
  sceneMgr_.Register(std::make_unique<TitleScene>());
  sceneMgr_.Register(std::make_unique<SelectScene>());
  sceneMgr_.Register(std::make_unique<GameScene>());
  sceneMgr_.Register(std::make_unique<ResultScene>());
  sceneMgr_.Register(std::make_unique<SampleScene>());

  // ===== 最初のシーンへ即時遷移 =====
#ifdef _DEBUG
  sceneMgr_.ChangeImmediately("Game", sceneCtx_);
#else
  sceneMgr_.ChangeImmediately("Title", sceneCtx_);
#endif // _DEBUG
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

  ImGui::Begin("Scene");
  const char *sceneNames[] = {"Title", "Select", "Game", "Result", "Sample"};
  const char *currentSceneName = sceneMgr_.CurrentName().c_str();

  if (ImGui::BeginCombo("##Scene", currentSceneName)) {
    for (int i = 0; i < IM_ARRAYSIZE(sceneNames); i++) {
      bool is_selected = (strcmp(currentSceneName, sceneNames[i]) == 0);
      if (ImGui::Selectable(sceneNames[i], is_selected)) {
        sceneMgr_.RequestChange(sceneNames[i]);
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::End();

  input_->Update();

  sceneMgr_.Update(sceneCtx_);
}

void App::Render() { sceneMgr_.Render(sceneCtx_, cl); }

void App::Term() {
  core_.WaitForGPU();
  pm_.Term();
  imgui_.Shutdown();

  core_.Term();

  RC::Term();

  if (input_) {
    input_.reset();
  }
  if (window_) {
    window_.reset();
  }
}
