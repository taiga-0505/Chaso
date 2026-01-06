#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"

void GameScene::OnEnter(SceneContext &ctx) {
  // ===== Camera =====
  camera_.Initialize(ctx.input, RC::Vector3{5.0f, 5.0f, -30.0f},
                     RC::Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  // ===== block =====
  blockModel = RC::LoadModel("Resources/model/block");
  RC::SetModelLightingMode(blockModel, None);

  // ===== MapChipField =====
  map_.SetBlockSize(kBlockSize);
  map_.RegisterTileDef(1, MapChipField::TileDef{.model = blockModel,
                                                .scale = kBlockSize,
                                                .flags = MapChipField::kSolid});
  map_.LoadFromCSV("Resources/Stage/Stage1/stage1.csv");
  map_.BuildInstances();

  // ===== Player =====
  playerModel = RC::LoadModel("Resources/model/player/player.obj");
  player_ = std::make_unique<Player>();
  player_->Init(playerModel, ctx);
  player_->SetMap(&map_);

  // ===== Skydome =====
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  skydomeModel = RC::GenerateSphereEx(txSphere_, 60.0f);
  sphereT_ = RC::GetSphereTransformPtr(skydomeModel);
  RC::SetSphereColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  // ===== 追従カメラ：ターゲットを渡したら追従ON =====
  if (Transform *pt = RC::GetModelTransformPtr(playerModel)) {
    camera_.SetTarget(pt);
  }

  // ===== Map bounds（カメラが外を映さない用）=====
  {
    const float s = map_.BlockSize();
    const float half = s * 0.5f;

    const float left = -half;
    const float right = (map_.Width() - 1) * s + half;
    const float bottom = -half;
    const float top = (map_.Height() - 1) * s + half;

    const bool enable = (map_.Width() > 0 && map_.Height() > 0);
    camera_.SetFollowBounds(left, right, bottom, top, enable);
  }
}

void GameScene::OnExit(SceneContext &) {
  RC::UnloadModel(playerModel);
  playerModel = -1;
  RC::UnloadModel(blockModel);
  blockModel = -1;
  RC::UnloadSphere(skydomeModel);
  skydomeModel = -1;
}

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {
  (void)sm;

#if RC_ENABLE_IMGUI
  camera_.DrawImGui();
  RC::DrawImGui3D(blockModel, "block");
#endif

  // === 天球回転 ===
  if (sphereT_) {
    sphereT_->rotation.y += 0.0005f;
  }

  player_->Update();

  const float dt = 1.0f / 60.0f;
  camera_.Update(dt);

  // 3) 描画用行列を更新
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();
  RC::SetCamera(view_, proj_, camera_.GetWorldPos());
}

void GameScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::DrawSphere(skydomeModel);

  player_->Draw();
  map_.Draw();

  RC::PreDraw2D(ctx, cl);
}
