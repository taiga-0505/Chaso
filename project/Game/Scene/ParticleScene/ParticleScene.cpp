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

  plane = RC::LoadModel("Resources/model/plane");

  particle_.Initialize(ctx.core->GetDevice());
}

void ParticleScene::OnExit(SceneContext &ctx) {

  RC::UnloadModel(plane);
  plane = -1;

  particle_.Finalize();
}

void ParticleScene::Update(SceneManager &sm, SceneContext &ctx) {

  //RC::DrawImGui3D(plane, "plane");

  // ===========================================
  // 更新処理
  // ===========================================

  camera_.Update();

  // viewとprojを渡す
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();

  RC::SetCamera(view_, proj_);

  particle_.Update(view_, proj_);
}

void ParticleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);

  //RC::DrawModel(plane);

  particle_.Render(cl);

  RC::PreDraw2D(ctx, cl);
}
