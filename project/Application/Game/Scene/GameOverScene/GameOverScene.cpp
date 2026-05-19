#include "GameOverScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "Graphics/PostProcess/PostProcess.h"

GameOverScene::~GameOverScene() {
  SceneContext dummy{};
  OnExit(dummy);
}
#include "imgui/imgui.h"

void GameOverScene::OnEnter(SceneContext &ctx) {
  // ======= カメラ初期化 =======
  // 視錐台パラメータ
  const float kNearZ = 0.1f;
  const float kFarZ = 1000.0f;
  camera_.Initialize(ctx.input, {0, 5, -20}, {0, 0, 0}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, kNearZ, kFarZ);

  // ======= スカイドーム生成 =======
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSkydomeEx(txSphere_, kSkyRadius);
  skydomeT_ = RC::GetSkydomeTransformPtr(skydomeModel);
  RC::SetSkydomeColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  gameOverModel = RC::LoadModel("Resources/UI/GameOver.obj");
  gameOverModelT_ = RC::GetModelTransformPtr(gameOverModel);
  tx_white = RC::LoadTex("Resources/white1x1.png");
  RC::SetModelColor(gameOverModel, {1.0f, 0.2f, 0.2f, 1.0f}); // 少し赤っぽくしておく
  // カメラ位置(0, 5, -20)に対して、正面の距離に配置する
  if (gameOverModelT_) {
    gameOverModelT_->translation = {0.0f, 5.0f, -10.0f};
    gameOverModelT_->scale = {1.0f, 1.0f, 1.0f};
    gameOverModelT_->rotation.y = 3.14159f;
    gameOverModelT_->rotation.x = 1.5708f;
  }

  // ======= ポストエフェクト =======
  if (ctx.postProcess) {
    ctx.postProcess->AddEffect(PostEffectType::Grayscale);
  }
}

void GameOverScene::OnExit(SceneContext &ctx) {
  if (ctx.postProcess) {
    ctx.postProcess->ClearEffects();
  }
  
  RC::UnloadModel(gameOverModel);
}

void GameOverScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  camera_.Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();
  RC::SetCamera(view_, proj_, camera_.GetWorldPos());

  // ======= スカイドーム更新 =======
  if (skydomeT_) {
    // カメラ座標に追従
    skydomeT_->translation = camera_.GetWorldPos();
    // 高さオフセット
    skydomeT_->translation.y -= 10.0f;
    // 自転処理
    skydomeT_->rotation.y += 0.0005f;
  }

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Title");
  }
}

void GameOverScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::DrawSkydome(skydomeModel);
  
  RC::DrawModel(gameOverModel, tx_white);

  RC::PreDraw2D(ctx, cl);
}
