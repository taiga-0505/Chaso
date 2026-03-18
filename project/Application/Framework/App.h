#pragma once
#include "AppConfig.h"
#include "Dx12Core.h"
#include "Game.h"
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ImGuiManager/ImGuiManager.h"
#include "Input/Input.h"
#include "PipelineManager.h"
#include "Scene.h"
#include "Window/Window.h"
#include <memory>

class TitleScene;
class SelectScene;
class GameScene;
class ResultScene;
class GameOverScene;
class SampleScene;

class App {
public:
  /// <summary>
  /// アプリケーション全体のライフサイクル管理と各サブシステムの初期化を担当する
  /// </summary>
  App();

  /// <summary>
  /// App を破棄する
  /// </summary>
  ~App();

  /// <summary>
  /// アプリケーションを初期化する
  /// </summary>
  bool Init();

  /// <summary>
  /// メインループを実行する
  /// </summary>
  int Run();

  /// <summary>
  /// アプリケーションを終了する
  /// </summary>
  void Term();

private:
  /// <summary>
  /// 1フレーム分の更新処理を行う
  /// </summary>
  void Update();

  /// <summary>
  /// 1フレーム分の描画処理を行う
  /// </summary>
  void Render();

private:
  ID3D12GraphicsCommandList *cl_ = nullptr;
  ID3D12Device *device_ = nullptr;

  // 設定 & ログ
  AppConfig appConfig_;

  // Window / DX12
  std::unique_ptr<Window> window_;
  Dx12Core core_;
  Dx12Core::Desc coreDesc_{};

  // 入力
  std::unique_ptr<Input> input_;

  // ImGui
  ImGuiManager imgui_;

  // パイプライン
  PipelineManager pm_;

  // === シーン管理 ===
  // === Game（シーン切替などゲーム側の責務） ===
  Game game_;
  SceneContext sceneCtx_;

  // Win32
  MSG msg_{};
};
