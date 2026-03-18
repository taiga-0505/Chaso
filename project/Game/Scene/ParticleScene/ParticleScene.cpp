#include "ParticleScene.h"
#include "Dx12/Dx12Core.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void ParticleScene::OnEnter(SceneContext &ctx) {
  // =============================
  // Camera
  // =============================

  camera_.Initialize(ctx.input, RC::Vector3{0.0f, 0.0f, -10.0f},
                     RC::Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  particle_.Initialize(ctx);

  fire_particle_.Initialize(ctx);

  rain_particle_.Initialize(ctx);

  snow_particle_.Initialize(ctx);

  circle_particle_.Initialize(ctx);

  explosion_particle_.Initialize(ctx);
  explosion_particle_.SetEmitterAutoSpawn(false);

  laser_particle_.Initialize(ctx);

  impact_particle_.Initialize(ctx);
  impact_particle_.SetEmitterAutoSpawn(false);

  wind_particle_.Initialize(ctx);
  wind_particle_.SetActive(false);

  impactDesc_.interval = 0.03f;
  impactDesc_.countPerTick = 6;
  impactDesc_.burstOnStart = 24;

  guide = RC::LoadSprite("Resources/guide.png", ctx, true);
  RC::SetSpriteScreenSize(guide, 330, 200);
}

void ParticleScene::OnExit(SceneContext &ctx) {
  particle_.Finalize();
  fire_particle_.Finalize();
  rain_particle_.Finalize();
  snow_particle_.Finalize();
  circle_particle_.Finalize();
  explosion_particle_.Finalize();
  laser_particle_.Finalize();
  impact_particle_.Finalize();
  wind_particle_.Finalize();
  RC::UnloadSprite(guide);
  guide = -1;
}

void ParticleScene::Update(SceneManager &sm, SceneContext &ctx) {

  particle_.DrawImGui();

  // ===========================================
  // 更新処理
  // ===========================================

  camera_.Update();

  // viewとprojを渡す
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();

  RC::SetCamera(view_, proj_, camera_.GetWorldPos());

  // ==============================
  // キー入力でパーティクル操作
  // ==============================
  Input *input = ctx.input;

  if (input->IsKeyTrigger(DIK_1)) {
    isParticle = true;
    isFire = false;
    isRain = false;
    isSnow = false;
    isCircle = false;
    isExplosion = false;
    isLaser = false;
    isWind = false;
    particle_.RespawnAllMax();
  }

  if (input->IsKeyTrigger(DIK_2)) {
    isParticle = false;
    isFire = true;
    isRain = false;
    isSnow = false;
    isCircle = false;
    isExplosion = false;
    isLaser = false;
    isWind = false;
    fire_particle_.RespawnAllMax();
  }

  if (input->IsKeyTrigger(DIK_3)) {
    isParticle = false;
    isFire = false;
    isRain = true;
    isSnow = false;
    isCircle = false;
    isExplosion = false;
    isLaser = false;
    isWind = false;
    rain_particle_.RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_4)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = true;
    isCircle = false;
    isExplosion = false;
    isLaser = false;
    isWind = false;
    snow_particle_.RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_6)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = false;
    isCircle = true;
    isExplosion = false;
    isLaser = false;
    isWind = false;
    circle_particle_.RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_5)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = false;
    isCircle = false;
    isExplosion = true;
    isLaser = false;
    isWind = false;
    explosion_particle_.RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_7)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = false;
    isCircle = false;
    isExplosion = false;
    isLaser = true;
    isWind = false;
  }
  if (input->IsKeyTrigger(DIK_8)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = false;
    isCircle = false;
    isExplosion = false;
    isLaser = false;
    isWind = true;
    wind_particle_.SetWind(windOrigin_, windForce_, windRange_, windRadius_,
                           true);
  }

  // IJKL + U/O でエミッタの位置を動かす
  //   A : -X方向, D : +X方向
  //   W : +Y方向, S : -Y方向
  //   Q : +Z方向, E : -Z方向
  const float emitterMoveSpeed = 0.05f;
  RC::Vector3 emitterDelta{0.0f, 0.0f, 0.0f};

  if (input->IsKeyPressed(DIK_A)) {
    emitterDelta.x -= emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_D)) {
    emitterDelta.x += emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_W)) {
    emitterDelta.y += emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_S)) {
    emitterDelta.y -= emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_Q)) {
    emitterDelta.z += emitterMoveSpeed;
  }
  if (input->IsKeyPressed(DIK_E)) {
    emitterDelta.z -= emitterMoveSpeed;
  }

  if (emitterDelta.x != 0.0f || emitterDelta.y != 0.0f ||
      emitterDelta.z != 0.0f) {
    if (isParticle) {
      particle_.MoveEmitter(emitterDelta);
    }
    if (isFire) {
      fire_particle_.MoveEmitter(emitterDelta);
    }
    if (isRain) {
      rain_particle_.MoveEmitter(emitterDelta);
    }
    if (isSnow) {
      snow_particle_.MoveEmitter(emitterDelta);
    }
    if (isCircle) {
      circle_particle_.MoveEmitter(emitterDelta);
    }
    if (isExplosion) {
      explosion_particle_.MoveEmitter(emitterDelta);
    }
    if (isWind) {
      wind_particle_.MoveEmitter(emitterDelta);
    }
  }

  if (input->IsKeyTrigger(DIK_SPACE)) {
    RC::Vector3 pos{0.0f, 0.0f, 0.0f};
    explosion_particle_.Trigger(pos);
  }

  if (isParticle) {
    particle_.Update(view_, proj_);
  }
  if (isFire) {
    fire_particle_.Update(view_, proj_);
  }
  if (isRain) {
    rain_particle_.Update(view_, proj_);
  }
  if (isSnow) {
    snow_particle_.Update(view_, proj_);
  }
  if (isCircle) {
    circle_particle_.Update(view_, proj_);
  }
  if (isExplosion) {
    explosion_particle_.Update(view_, proj_);
  }
  if (isWind) {
    wind_particle_.Update(view_, proj_);
  }
  if (isLaser) {
    ImGui::Begin("Laser Control");
    ImGui::DragFloat3("Start", &laserStart_.x, 0.1f);
    ImGui::DragFloat3("End", &laserEnd_.x, 0.1f);
    ImGui::DragFloat("Width", &laserWidth_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Life", &laserLife_, 0.01f, 0.01f, 10.0f);
    ImGui::ColorEdit4("Color", &laserColor_.x);
    ImGui::Separator();

    ImGui::Text("streak settings");
    ImGui::DragInt("Streak Count", &streakCount_, 1, 1, 500);
    ImGui::DragFloat("Streak Min Length", &streakMinLen_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Streak Max Length", &streakMaxLen_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Streak Width", &streakWidth_, 0.001f, 0.001f, 1.0f);
    ImGui::DragFloat("Streak Spread Z Ratio", &streakSpreadZRatio_, 0.01f, 0.0f,
                     1.0f);

    ImGui::Separator();

    ImGui::Text("impact particle settings");
    ImGui::DragFloat("Impact Interval", &impactDesc_.interval, 0.01f, 0.0f,
                     1.0f);
    ImGui::DragInt("Count Per Tick", &impactDesc_.countPerTick, 1, 1, 100);
    ImGui::DragInt("Burst On Start", &impactDesc_.burstOnStart, 1, 0, 100);

    ImGui::End();

    laser_particle_.SetStyle(RC::LaserParticle::Style::Streak);
    laser_particle_.SetStreakParams(
        streakCount_,       // streakCount
        streakMinLen_,      // minLen
        streakMaxLen_,      // maxLen
        streakWidth_,       // streakWidth（細さ）
        streakSpreadZRatio_ // spreadZRatio（奥行き散らし、縦筋なら小さめがオススメ）
    );

    laser_particle_.SetBeam(laserStart_, laserEnd_, laserWidth_, laserLife_,
                            laserColor_);
    laser_particle_.Update(view_, proj_);

    impact_particle_.SetImpactFromBeam(laserStart_, laserEnd_, impactDesc_);
    impact_particle_.Update(view_, proj_);
  } else {
    impact_particle_.StopImpact();
  }
}

void ParticleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);

  if (isParticle) {
    particle_.Render(ctx, cl);
  }
  if (isFire) {
    fire_particle_.Render(ctx, cl);
  }
  if (isRain) {
    rain_particle_.Render(ctx, cl);
  }
  if (isSnow) {
    snow_particle_.Render(ctx, cl);
  }
  if (isCircle) {
    circle_particle_.Render(ctx, cl);
  }
  if (isExplosion) {
    explosion_particle_.Render(ctx, cl);
  }
  if (isWind) {
    wind_particle_.Render(ctx, cl);
  }
  if (isLaser) {
    laser_particle_.Render(ctx, cl);
    impact_particle_.Render(ctx, cl);
  }

  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(guide);
}

ParticleScene::~ParticleScene() {
  if (guide >= 0) {
    RC::UnloadSprite(guide);
    guide = -1;
  }
}
