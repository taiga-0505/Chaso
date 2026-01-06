#include "GameScene.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include <cmath> // tanf, sqrtf

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


  // カメラ初期位置記録
  if (Transform *pt = RC::GetModelTransformPtr(playerModel)) {
    prevPlayerPos_ = pt->translation;
    camFollowInit_ = false;
  }

  // ===== Map bounds（カメラが外を映さない用）=====
  {
    const float s = map_.BlockSize(); // 今は 1.0f のはず
    const float half = s * 0.5f;

    // タイル中心が (0..W-1, 0..H-1) に並ぶ前提で “端の面” まで含める
    mapBounds_.left = -half;
    mapBounds_.right = (map_.Width() - 1) * s + half;
    mapBounds_.bottom = -half;
    mapBounds_.top = (map_.Height() - 1) * s + half;

    mapBoundsReady_ = (map_.Width() > 0 && map_.Height() > 0);
  }

  camFovY_ = 0.45f;
  camAspect_ = float(ctx.app->width) / ctx.app->height;
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

#if RC_ENABLE_IMGUI
  camera_.DrawImGui();
  RC::DrawImGui3D(blockModel, "block");
#endif

  // === 天球回転 ===
  if (sphereT_) {
    sphereT_->rotation.y += 0.0005f;
  }

  // 1) プレイヤーを先に更新（最新座標が欲しい）
  player_->Update();

  const float dt = 1.0f / 60.0f;

  if (Transform *pt = RC::GetModelTransformPtr(playerModel)) {
    const RC::Vector3 playerPos = pt->translation;

    // --- 先読みの「目標」（瞬間値） ---
    const float dx = playerPos.x - prevPlayerPos_.x;
    const float desiredLookAheadX =
        RC::Clamp(dx * camLookAheadFactor_, -camLookAhead_, camLookAhead_);

    // 初回だけスナップ
    if (!camFollowInit_) {
      camLookAheadX_ = desiredLookAheadX;

      camFocus_ = RC::Add(playerPos, camTargetOffset_);
      camFocus_ = RC::Add(camFocus_, RC::Vector3{camLookAheadX_, 0.0f, 0.0f});
    } else {
      // ===== 先読みの現在値をなめらかに =====
      {
        const float tLA = RC::ExpSmoothingFactor(camLookAheadSharpness_, dt);
        camLookAheadX_ =
            camLookAheadX_ + (desiredLookAheadX - camLookAheadX_) * tLA;
      }

      // ===== 追従したい注視点（目標） =====
      const RC::Vector3 desiredFocus =
          RC::Add(RC::Add(playerPos, camTargetOffset_),
                  RC::Vector3{camLookAheadX_, 0.0f, 0.0f});

      // ===== X/Z は今まで通りなめらかに =====
      const float tF = RC::ExpSmoothingFactor(camFocusSharpness_, dt);
      camFocus_.x = camFocus_.x + (desiredFocus.x - camFocus_.x) * tF;
      camFocus_.z = camFocus_.z + (desiredFocus.z - camFocus_.z) * tF;

      // ===== Y は「デッドゾーン + 上下で追従速度を変える」 =====
      const float dy = desiredFocus.y - camFocus_.y;

      float targetY = camFocus_.y;
      if (std::fabs(dy) > camDeadZoneY_) {
        // デッドゾーン外に出たら、ゾーン端までだけ追いかける
        targetY = desiredFocus.y - (dy > 0.0f ? camDeadZoneY_ : -camDeadZoneY_);
      }

      const float sharpY =
          (targetY > camFocus_.y) ? camYSharpnessUp_ : camYSharpnessDown_;
      const float tY = RC::ExpSmoothingFactor(sharpY, dt);
      float newY = camFocus_.y + (targetY - camFocus_.y) * tY;

      // 最大速度制限（酔い＆ガクガク対策）
      if (camYMaxSpeed_ > 0.0f) {
        const float maxStep = camYMaxSpeed_ * dt;
        const float step = newY - camFocus_.y;
        if (step > maxStep)
          newY = camFocus_.y + maxStep;
        if (step < -maxStep)
          newY = camFocus_.y - maxStep;
      }

      camFocus_.y = newY;
    }


    RC::Vector3 focus = camFocus_;

    // ===== focus をマップ境界でクランプ =====
    if (mapBoundsReady_) {
      const RC::Vector3 delta = RC::Sub(camOffset_, camTargetOffset_);
      const float dist =
          std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

      const float halfH = std::tanf(camFovY_ * 0.5f) * dist;
      const float halfW = halfH * camAspect_;

      const float minX = mapBounds_.left + halfW;
      const float maxX = mapBounds_.right - halfW;
      const float minY = mapBounds_.bottom + halfH;
      const float maxY = mapBounds_.top - halfH;

      if (minX > maxX) {
        focus.x = (mapBounds_.left + mapBounds_.right) * 0.5f;
      } else {
        focus.x = RC::Clamp(focus.x, minX, maxX);
      }
      if (minY > maxY) {
        focus.y = (mapBounds_.bottom + mapBounds_.top) * 0.5f;
      } else {
        focus.y = RC::Clamp(focus.y, minY, maxY);
      }
    }

    // クランプ結果を状態にも戻す（壁際での安定が上がる）
    camFocus_ = focus;

    // ===== カメラ位置を focus から決める（offset維持）=====
    const RC::Vector3 delta = RC::Sub(camOffset_, camTargetOffset_);
    const RC::Vector3 desiredPos = RC::Add(focus, delta);

    if (!camFollowInit_) {
      camPos_ = desiredPos;
      camFollowInit_ = true;
    } else {
      const float t = RC::ExpSmoothingFactor(camSharpness_, dt);
      camPos_ = RC::Lerp(camPos_, desiredPos, t);
    }

    // 注視点へ向ける（※smoothしたfocusを使う）
    const RC::Vector3 rot = RC::CameraMath::LookAtYawPitch(camPos_, focus);

    camera_.SetMainPosition(camPos_);
    camera_.SetMainRotation(rot);

    prevPlayerPos_ = playerPos;
  }


  // 3) 最後にカメラ更新（Tab切替もここで反映）
  camera_.Update();

  // 4) 描画用行列を更新
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
