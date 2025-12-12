#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void GameScene::OnEnter(SceneContext &ctx) {
  // ===== Camera =====
  camera_.Initialize(ctx.input, RC::Vector3{5.0f, 5.0f, -30.0f},
                     RC::Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  // ===== block =====

  blockModel = RC::LoadModel("Resources/model/block");
  RC::SetModelLightingMode(blockModel, None);

  // ===== MapChipField setup =====
  map_.SetBlockSize(kBlockSize);

  map_.RegisterTileDef(1, MapChipField::TileDef{.model = blockModel,
                                                .scale = kBlockSize,
                                                .flags = MapChipField::kSolid});

  map_.LoadFromCSV("Resources/Stage/Stage1/stage01.csv");
  map_.BuildInstances();

  // ===== Player =====

  playerModel = RC::LoadModel("Resources/model/player/player.obj");
  player_ = std::make_unique<Player>();
  player_->Init(playerModel, ctx);
}

void GameScene::OnExit(SceneContext &) {
  RC::UnloadModel(playerModel);
  RC::UnloadModel(blockModel);
}

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {

  camera_.DrawImGui();
  camera_.Update();

  RC::DrawImGui3D(blockModel, "block");

  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();
  RC::SetCamera(view_, proj_, camera_.GetWorldPos());

  player_->Update();
}

void GameScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  player_->Draw();
  map_.Draw();

  RC::PreDraw2D(ctx, cl);
}
