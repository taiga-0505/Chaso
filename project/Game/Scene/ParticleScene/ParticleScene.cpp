#include "ParticleScene.h"
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
    particle_.RespawnAllMax();
  }

  if (input->IsKeyTrigger(DIK_2)) {
    isParticle = false;
    isFire = true;
    isRain = false;
    isSnow = false;
    isCircle = false;
    isExplosion = false;
    fire_particle_.RespawnAllMax();
  }

  if (input->IsKeyTrigger(DIK_3)) {
    isParticle = false;
    isFire = false;
    isRain = true;
    isSnow = false;
    isCircle = false;
    isExplosion = false;
    rain_particle_.RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_4)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = true;
    isCircle = false;
    isExplosion = false;
    snow_particle_.RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_6)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = false;
    isCircle = true;
    isExplosion = false;
    circle_particle_.RespawnAllMax();
  }
  if (input->IsKeyTrigger(DIK_5)) {
    isParticle = false;
    isFire = false;
    isRain = false;
    isSnow = false;
    isCircle = false;
    isExplosion = true;
    explosion_particle_.RespawnAllMax();
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
  }

  if (input->IsKeyTrigger(DIK_SPACE)) {
    RC::Vector3 pos{ 0.0f, 0.0f, 0.0f};
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
  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(guide);
}
