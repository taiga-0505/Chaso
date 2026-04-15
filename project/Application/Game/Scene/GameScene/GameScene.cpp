#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"

GameScene::~GameScene() {
  SceneContext dummy{};
  OnExit(dummy);
}

void GameScene::OnEnter(SceneContext &ctx) {
  // ==============
  // シーン初期化
  // ==============

  // ======= カメラ初期化 =======
  // 視錐台パラメータ
  const float kNearZ = 0.1f;
  const float kFarZ = 100.0f;
  camera_.Initialize(ctx.input, {5, 5, -30}, {0, 0, 0}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, kNearZ, kFarZ);

}

void GameScene::OnExit(SceneContext &) {
}

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {
#if RC_ENABLE_IMGUI
  camera_.DrawImGui();
#endif

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  camera_.Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();
  RC::SetCamera(view_, proj_, camera_.GetWorldPos());
}

void GameScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // ==============
  // 描画処理
  // ==============

  // ======= 3D描画 =======
  RC::PreDraw3D(ctx, cl);

  // ======= 2D描画準備 =======
  RC::PreDraw2D(ctx, cl);

}
