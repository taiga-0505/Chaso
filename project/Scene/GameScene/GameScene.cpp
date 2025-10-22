#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void GameScene::OnEnter(SceneContext &ctx) {
  ID3D12Device *device = ctx.core->GetDevice();
  auto &srvHeap = ctx.core->SRV();

  // ===== Camera =====
  camera_.Initialize(ctx.input, Vector3{0.0f, 0.0f, -5.0f},
                     Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);
}

void GameScene::OnExit(SceneContext &) {}

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {

  camera_.DrawImGui();
  camera_.Update();

  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();
}

void GameScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);
}
