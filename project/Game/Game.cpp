#include "Game.h"
#include "imgui/imgui.h"

// === 各シーン ===
#include "GameOverScene/GameOverScene.h"
#include "GameScene/GameScene.h"
#include "ResultScene/ResultScene.h"
#include "SampleScene/SampleScene.h"
#include "SelectScene/SelectScene.h"
#include "TitleScene/TitleScene.h"
#include "ParticleScene/ParticleScene.h"
#include "LightScene/LightScene.h"

void Game::Init(SceneContext &ctx) {
  sceneMgr_.Init(ctx);
  registerScenes_();

  // ここで最初のシーンを決める（Gameの責務）
#if defined(RC_DEVELOPMENT)
  const char *boot = "Light";
#elif defined(_DEBUG)
  const char *boot = "Sample";
#else
  const char *boot = "Title";
#endif
  sceneMgr_.ChangeImmediately(boot, ctx);
}

void Game::registerScenes_() {
  sceneMgr_.Register(std::make_unique<TitleScene>());
  sceneMgr_.Register(std::make_unique<SelectScene>());
  sceneMgr_.Register(std::make_unique<GameScene>());
  sceneMgr_.Register(std::make_unique<ResultScene>());
  sceneMgr_.Register(std::make_unique<GameOverScene>());
  sceneMgr_.Register(std::make_unique<SampleScene>());
  sceneMgr_.Register(std::make_unique<ParticleScene>());
  sceneMgr_.Register(std::make_unique<LightScene>());
}

void Game::registerAudioPaths_() {
  // --- BGM ---
  // bgm_.SetPath(BgmGroup::Main, "Resources/audio/bgm_main.mp3");
  // bgm_.SetPath(BgmGroup::Clear, "Resources/audio/bgm_result.mp3");
  // bgm_.SetPath(BgmGroup::GameOver, "Resources/audio/bgm_gameover.mp3");

  // --- SE ---
  // se_.SetPath(SeId::Cursor, "Resources/audio/se_cursor.wav");
  // se_.SetPath(SeId::Decide, "Resources/audio/se_decide.wav");
  // se_.SetPath(SeId::Cancel, "Resources/audio/se_cancel.wav");
  // se_.SetPath(SeId::Hit, "Resources/audio/se_hit.wav");
  // se_.SetPath(SeId::Damage, "Resources/audio/se_damage.wav");
  // se_.SetPath(SeId::Clear, "Resources/audio/se_clear.wav");
}

void Game::Update(SceneContext &ctx) {
    sceneMgr_.Update(ctx); 
}

void Game::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  sceneMgr_.Render(ctx, cl);
}

void Game::Term() {
  sceneMgr_.Term();
  se_.Term();
  bgm_.Stop();
}

void Game::RequestChange(const std::string &name) {
  sceneMgr_.RequestChange(name);
}

const std::string &Game::CurrentSceneName() const {
  return sceneMgr_.CurrentName();
}

void Game::DrawDebugUI() {
#ifdef _DEBUG
  ImGui::Begin("Scene");
  const char *sceneNames[] = {"Title",  "Select",   "Game",      "Result",
                              "GameOver", "Sample", "Particle", "Light"};
  const char *currentSceneName = CurrentSceneName().c_str();

  if (ImGui::BeginCombo("##Scene", currentSceneName)) {
    for (int i = 0; i < IM_ARRAYSIZE(sceneNames); i++) {
      bool is_selected = (strcmp(currentSceneName, sceneNames[i]) == 0);
      if (ImGui::Selectable(sceneNames[i], is_selected)) {
        RequestChange(sceneNames[i]);
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::End();

  // === FPS overlay ===
  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowBgAlpha(0.35f);
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav;
  if (ImGui::Begin("Perf", nullptr, flags)) {
    const float fps = io.Framerate;
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame: %.3f ms", 1000.0f / (fps > 0.0f ? fps : 1.0f));
  }
  ImGui::End();

#endif
  
}
