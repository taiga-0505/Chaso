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
}

void ParticleScene::OnExit(SceneContext &ctx) {

  particle_.Finalize();
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

  RC::SetCamera(view_, proj_);

  // ==============================
  // キー入力でパーティクル操作
  // ==============================
  Input *input = ctx.input;

  // Space : Update を止めたり / 再開したり
  if (input->IsKeyTrigger(DIK_SPACE)) {
    particle_.ToggleEnableUpdate();
  }

  // 1キー : 風（加速度フィールド）ON/OFF
  if (input->IsKeyTrigger(DIK_1)) {
    particle_.ToggleUseAccelerationField();
  }

  // 3キー : 全部リスポーン（最大数で再生成）
  if (input->IsKeyTrigger(DIK_2)) {
    particle_.RespawnAllMax();
  }

  // 3キー : 全パーティクル削除
  if (input->IsKeyTrigger(DIK_3)) {
    particle_.ClearAll();
  }

  // 4キー : エミッタからパーティクル追加発生
  if (input->IsKeyTrigger(DIK_4)) {
    particle_.AddParticlesFromEmitter();
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
    particle_.MoveEmitter(emitterDelta);
  }

  particle_.Update(view_, proj_);
}

void ParticleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);
  
  particle_.Render(ctx,cl);

  RC::PreDraw2D(ctx, cl);
}
