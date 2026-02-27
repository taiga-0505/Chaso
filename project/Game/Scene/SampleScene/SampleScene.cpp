#include "SampleScene.h"
#include "Dx12Core.h"
#include "Input/Input.h"
#include "PipelineManager.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void SampleScene::OnEnter(SceneContext &ctx) {

  // =============================
  // Camera
  // =============================

  camera_.Initialize(ctx.input, RC::Vector3{0.0f, 0.35f, -15.0f},
                     RC::Vector3{0.0f, -0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  // =============================
  // Light初期化
  // =============================

  directionalLight = RC::CreateDirectionalLight();

  if (RC::DirectionalLightSource *sun =
          RC::GetDirectionalLightPtr(directionalLight)) {
    sun->SetDirection({0.0f, -1.0f, 0.2f});   // ちょいナナメ上
    sun->SetColor({1.0f, 0.95f, 0.9f, 1.0f}); // ほんのり暖色
    sun->SetIntensity(1.5f);
  }

  pointLight = RC::CreatePointLight();

  pointLight2 = RC::CreatePointLight();

  spotLight = RC::CreateSpotLight();

  spotLight2 = RC::CreateSpotLight();

  // =============================
  // モデル初期化
  // =============================

  plane = RC::LoadModel("Resources/model/plane");
  planeTransform_ = RC::GetModelTransformPtr(plane);

  blockModel = RC::LoadModel("Resources/model/block");
  RC::SetModelColor(blockModel, blockColor_); // ちょい青で透明

  model = RC::LoadModel("Resources/model/teapot");
  tx_model = RC::LoadTex("Resources/uvChecker.png");

  terrain = RC::LoadModel("Resources/model/terrain");
  terrainT_ = RC::GetModelTransformPtr(terrain);
  terrainT_->translation.y = -1.0f;

  // 天球
  tx_Sphere_ = RC::LoadTex("Resources/skydome.jpg");
  sphere = RC::GenerateSphereEx(tx_Sphere_, 40.0f);
  sphereT_ = RC::GetSphereTransformPtr(sphere);
  RC::SetSphereColor(sphere, {0.6f, 1.0f, 1.0f, 1.0f});

  tx_ball = RC::LoadTex("Resources/monsterBall.png");
  ball = RC::GenerateSphereEx(tx_ball, 1.0f, 16, 16, false);
  ballT_ = RC::GetSphereTransformPtr(ball);
  RC::SetSphereColor(ball, {1.0f, 1.0f, 1.0f, 1.0f});
  ballT_->rotation.y = -1.6f;

  // =============================
  // Sprite初期化
  // =============================

  sprite = RC::LoadSprite("Resources/uvChecker.png", ctx);
  RC::SetSpriteTransform(
      sprite, {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {100.0f, 100.0f, 0.0f}});
  RC::SetSpriteScreenSize(sprite, 500.0f, 100.0f);
}

void SampleScene::OnExit(SceneContext &) {

  RC::UnloadModel(plane);
  plane = -1;
  RC::UnloadModel(model);
  model = -1;
  RC::UnloadModel(blockModel);
  blockModel = -1;

  RC::UnloadModel(terrain);
  terrain = -1;

  RC::UnloadSphere(sphere);
  sphere = -1;

  RC::UnloadSprite(sprite);
  sprite = -1;

  RC::UnloadSphere(ball);
  ball = -1;

  RC::DestroyDirectionalLight(directionalLight);
  directionalLight = -1;
  RC::DestroyPointLight(pointLight);
  pointLight = -1;
  RC::DestroyPointLight(pointLight2);
  pointLight2 = -1;
  RC::DestroySpotLight(spotLight);
  spotLight = -1;
  RC::DestroySpotLight(spotLight2);
  spotLight2 = -1;
}

void SampleScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ===========================================
  // ImGui
  // ===========================================

#if RC_ENABLE_IMGUI

  DrawImGui();
  RC::SetModelColor(blockModel, blockColor_);

  camera_.DrawImGui();

#endif // _DEBUG

  // ===========================================
  // 更新処理
  // ===========================================

  t += 1.0f / 60.0f;

  camera_.Update();

  planeTransform_->rotation.y += 0.01f;

  // viewとprojを渡す
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();

  RC::SetCamera(view_, proj_, camera_.GetWorldPos());

  // === 天球回転 ===
  sphereT_->rotation.y += 0.001f;
}

void SampleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // ===========================================
  // 3D描画
  // ===========================================
  RC::PreDraw3D(ctx, cl);

  // === 天球 ===
  RC::DrawSphere(sphere);

  // RC::DrawSphere(ball);

  // モデルの描画
  RC::DrawModel(plane);

  RC::DrawModel(model, tx_model);

  RC::DrawModel(terrain);

  RC::DrawModelGlassTwoPass(blockModel);

  // ===========================================
  // 2D描画
  // ===========================================
  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(sprite);

  RC::SetFogOverlayColor(fogColor_); // ちょい青
  if (isFogEnabled_) {
    RC::DrawFogOverlay(t,
                       0.55f,                    // intensity
                       4.0f,                     // scale
                       3.5f,                     // speed
                       RC::Vector2{0.08f, 0.0f}, // wind
                       0.18f,                    // feather
                       0.35f                     // bottomBias
    );
  }
}

void SampleScene::DrawImGui() {
  ImGui::Begin("Debug");

  if (ImGui::BeginTabBar("MainDebugTabBar")) {
    // -------------------
    // ModelTab
    // -------------------
    if (ImGui::BeginTabItem("ModelTab")) {

      RC::DrawImGui3D(terrain, "terrain");

      RC::DrawImGui3D(plane, "plane");

      RC::DrawImGui3D(model, "model");

      RC::DrawSphereImGui(sphere, "skyDome");

      RC::DrawSphereImGui(ball, "ball");

      RC::DrawImGui3D(blockModel, "blockModel");
      // 色変更用
      ImGui::ColorEdit4("blockColor", &blockColor_.x);

      ImGui::EndTabItem();
    }

    // -------------------
    // SpriteTab
    // -------------------
    if (ImGui::BeginTabItem("SpriteTab")) {

      RC::DrawImGui2D(sprite, "sprite");

      ImGui::EndTabItem();
    }

    // -------------------
    // SoundTab
    // -------------------
    if (ImGui::BeginTabItem("SoundTab")) {
      ImGui::EndTabItem();
    }

    // -------------------
    // POSTEffectTab
    // -------------------
    if (ImGui::BeginTabItem("PostEffectTab")) {

      // -------------------
      // Fog
      // -------------------
      ImGui::Checkbox("isFogEnabled", &isFogEnabled_);
      ImGui::ColorEdit4("fogColor", &fogColor_.x);

      ImGui::EndTabItem();
    }

    // -------------------
    // LightTab
    // -------------------
    if (ImGui::BeginTabItem("LightTab")) {
      RC::DrawImGuiDirectionalLight(directionalLight, "DirectionalLight");
      RC::DrawImGuiPointLight(pointLight, "PointLight");
      RC::DrawImGuiPointLight(pointLight2, "PointLight2");
      RC::DrawImGuiSpotLight(spotLight, "SpotLight");
      RC::DrawImGuiSpotLight(spotLight2, "SpotLight2");
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}
