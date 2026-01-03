#pragma once
#include "Scene.h"
#include "SceneManager.h"
#include "Sound/BgmManager.h"
#include "Sound/SeManager.h"
#include <string>

class Game {
public:
  Game() = default;
  ~Game() = default;

  // 最初に遷移するシーン名を渡す
  void Init(SceneContext &ctx);
  void Update(SceneContext &ctx);
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);
  void Term();

  // デバッグ用（ImGui）シーン切替UI
  void DrawDebugUI();

  // 外部からシーン遷移を要求したいとき用
  void RequestChange(const std::string &name);
  const std::string &CurrentSceneName() const;

private:
  void registerScenes_();     // シーン登録をひとまとめに
  void registerAudioPaths_(); // オーディオパス登録をひとまとめに

private:
  Scene::SceneManager sceneMgr_;
  BgmManager bgm_;
  SeManager se_;
};
