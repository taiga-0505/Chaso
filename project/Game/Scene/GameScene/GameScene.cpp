#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"

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

  // ======= ブロックモデル =======
  blockModel = RC::LoadModel("Resources/model/block");
  RC::SetModelLightingMode(blockModel, None);

  // ======= マップ構築 =======
  map_.SetBlockSize(kBlockSize);
  map_.RegisterTileDef(1, MapChipField::TileDef{.model = blockModel,
                                                .scale = kBlockSize,
                                                .flags = MapChipField::kSolid});
  map_.LoadFromCSV("Resources/Stage/Stage1/stage1.csv");
  map_.BuildInstances();

  // ======= プレイヤー生成 =======
  playerModel = RC::LoadModel("Resources/model/player/player.obj");
  player_ = std::make_unique<Player>();
  player_->Init(playerModel, ctx);
  player_->SetMap(&map_);

  // ======= コイン生成 =======
  {
    // コインモデルパス
    const char *kCoinModelPath = "Resources/model/coin/coin.obj";
    // 出現セル一覧
    const auto &spawns = map_.CoinSpawns();
    coins_.reserve(spawns.size());

    for (const auto &idx : spawns) {
      int coinModel = RC::LoadModel(kCoinModelPath);

      auto coin = std::make_unique<Coin>();
      coin->Init(coinModel, ctx);

      // コイン中心座標
      RC::Vector3 pos = map_.IndexToCenter(idx);
      // 浮かせたい場合は pos.y += 0.3f のように調整
      coin->SetWorldPos(pos);

      coins_.push_back(std::move(coin));
    }

    reachedGoal_ = false;

    // ゴール生成
    {
      const char *kGoalModelPath = "Resources/model/Goal/Goal.obj";

      const auto &spawns = map_.GoalSpawns();
      goals_.reserve(spawns.size());

      for (const auto &idx : spawns) {
        int goalModel = RC::LoadModel(kGoalModelPath);

        auto goal = std::make_unique<Goal>();
        goal->Init(goalModel);

        RC::Vector3 pos = map_.IndexToCenter(idx);
        goal->SetWorldPos(pos);

        goals_.push_back(std::move(goal));
      }
    }

  }

  // ======= スカイドーム生成 =======
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSphereEx(txSphere_, kSkyRadius);
  sphereT_ = RC::GetSphereTransformPtr(skydomeModel);
  RC::SetSphereColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  // ======= 追従カメラ設定 =======
  if (Transform *pt = RC::GetModelTransformPtr(playerModel)) {
    // プレイヤーモデルをターゲットに設定
    camera_.SetTarget(pt);
  }

  // ======= マップ境界設定 =======
  {
    // ブロック寸法
    const float s = map_.BlockSize();
    const float half = s * 0.5f;

    // 視界制限境界
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
  for (auto &c : coins_) {
    if (!c)
      continue;
    RC::UnloadModel(c->ModelHandle());
  }
  coins_.clear();

  for (auto &g : goals_) {
    if (!g)
      continue;
    RC::UnloadModel(g->ModelHandle());
  }
  goals_.clear();
}

void GameScene::Update(SceneManager &sm, SceneContext &ctx) {
  (void)sm;

#if RC_ENABLE_IMGUI
  camera_.DrawImGui();
  RC::DrawImGui3D(blockModel, "block");
#endif

  // ======= スカイドーム更新 =======
  if (sphereT_) {
    // カメラ座標に追従
    sphereT_->translation = camera_.GetWorldPos();
    // 高さオフセット
    sphereT_->translation.y -= 10.0f;
    // 自転処理
    sphereT_->rotation.y += 0.0005f;
  }

  // ======= エンティティ更新 =======
  // プレイヤー更新
  player_->Update();
  // コイン更新
  for (auto &c : coins_) {
    if (c)
      c->Update();
  }
  // ゴール更新
  for (auto &g : goals_) {
    if (g)
      g->Update();
  }

  // ======= コイン取得判定 =======
  {
    // プレイヤー座標
    const RC::Vector3 p = player_->GetWorldPos();

    for (auto &c : coins_) {
      if (!c)
        continue;
      if (!c->IsAlive() || c->IsCollected())
        continue;

      // コイン座標
      const RC::Vector3 cp = c->GetWorldPos();
      // 2D距離
      const float dx = std::abs(p.x - cp.x);
      const float dy = std::abs(p.y - cp.y);

      // 当たり判定
      if (dx < 0.6f && dy < 0.6f) {
        player_->GetCoin(1);
        c->GetCoin();
      }
    }

    // 消失コイン整理
    for (auto it = coins_.begin(); it != coins_.end();) {
      if (*it && !(*it)->IsAlive()) {
        RC::UnloadModel((*it)->ModelHandle());
        it = coins_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // ゴール到達判定
  if (!reachedGoal_) {
    const RC::Vector3 p = player_->GetWorldPos();

    for (auto &g : goals_) {
      if (!g)
        continue;
      if (g->IsReached())
        continue;

      const RC::Vector3 gp = g->GetWorldPos();
      const float dx = std::abs(p.x - gp.x);
      const float dy = std::abs(p.y - gp.y);

      if (dx < 0.6f && dy < 0.6f) {
        reachedGoal_ = true;
        g->Reach();
        sm.RequestChange("Title");
        break;
      }
    }
  }

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
  RC::DrawSphere(skydomeModel);

  player_->Draw();
  for (auto &c : coins_) {
    if (c)
      c->Draw();
  }

  for (auto &g : goals_) {
    if (g)
      g->Draw();
  }

  map_.Draw();

  // ======= 2D描画準備 =======
  RC::PreDraw2D(ctx, cl);
}
