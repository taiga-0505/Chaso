#include "LightScene.h"
#include "Dx12/Dx12Core.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void LightScene::OnEnter(SceneContext &ctx) {
  // =============================
  // Camera
  // =============================

  camera_.Initialize(ctx.input, RC::Vector3{0.0f, 0.35f, -15.0f},
                     RC::Vector3{0.0f, -0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  terrain_ = RC::LoadModel("Resources/model/terrain");
  terrainT_ = RC::GetModelTransformPtr(terrain_);
  terrainT_->translation.y = -1.0f;

  ball_ = RC::LoadModel("Resources/model/monsterBall");
  ballT_ = RC::GetModelTransformPtr(ball_);
  ballT_->rotation.y = -1.6f;

  DirectionalLight_ = RC::CreateDirectionalLight();
  RC::SetActiveDirectionalLight(DirectionalLight_);

  SpotLight1_ = RC::CreateSpotLight();

  SpotLight2_ = RC::CreateSpotLight();

  PointLight1_ = RC::CreatePointLight();

  PointLight2_ = RC::CreatePointLight();
}

void LightScene::OnExit(SceneContext &ctx) {

    RC::UnloadModel(ball_);

}

void LightScene::Update(SceneManager &sm, SceneContext &ctx) {

  DrawImGui();

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
  }

  if (input->IsKeyTrigger(DIK_2)) {
  }

  if (input->IsKeyTrigger(DIK_3)) {
  }
  if (input->IsKeyTrigger(DIK_4)) {
  }

  if (input->IsKeyTrigger(DIK_5)) {
  }

  if (input->IsKeyPressed(DIK_A)) {
  }
  if (input->IsKeyPressed(DIK_D)) {
  }
  if (input->IsKeyPressed(DIK_W)) {
  }
  if (input->IsKeyPressed(DIK_S)) {
  }
}

void LightScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);

  RC::DrawModel(ball_);

  RC::DrawModel(terrain_);

  RC::PreDraw2D(ctx, cl);
}

void LightScene::DrawImGui() {
  camera_.DrawImGui();

  ImGui::Begin("Debug");

  if (ImGui::BeginTabBar("MainDebugTabBar")) {
    // -------------------
    // ModelTab
    // -------------------
    if (ImGui::BeginTabItem("ModelTab")) {

      RC::DrawImGui3D(terrain_, "Terrain");
      RC::DrawImGui3D(ball_, "Ball");

      ImGui::EndTabItem();
    }

    // -------------------
    // SpriteTab
    // -------------------
    if (ImGui::BeginTabItem("SpriteTab")) {

      ImGui::EndTabItem();
    }

    // -------------------
    // LightTab
    // -------------------
    if (ImGui::BeginTabItem("LightTab")) {

      RC::DrawImGuiDirectionalLight(DirectionalLight_, "DirectionalLight");
      RC::DrawImGuiPointLight(PointLight1_, "PointLight1");
      RC::DrawImGuiPointLight(PointLight2_, "PointLight2");
      RC::DrawImGuiSpotLight(SpotLight1_, "SpotLight1");
      RC::DrawImGuiSpotLight(SpotLight2_, "SpotLight2");

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}
