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

  camera_.Initialize(ctx.input, RC::Vector3{0.0f, 0.0f, -10.0f},
                     RC::Vector3{0.0f, 0.0f, 0.0f}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, 0.1f, 100.0f);

  // =============================
  // Light初期化
  // =============================

  light = RC::CreateLight();
  RC::SetActiveLight(light);

  if (auto *sun = RC::GetLightPtr(light)) {
    sun->SetDirection({0.0f, -1.0f, 0.2f});   // ちょいナナメ上
    sun->SetColor({1.0f, 0.95f, 0.9f, 1.0f}); // ほんのり暖色
    sun->SetIntensity(1.5f);
  }

  // =============================
  // モデル初期化
  // =============================

  plane = RC::LoadModel("Resources/model/plane");
  planeTransform_ = RC::GetModelTransformPtr(plane);

  blockModel = RC::LoadModel("Resources/model/cube");

  model = RC::LoadModel("Resources/model/teapot");
  tx_model = RC::LoadTex("Resources/uvChecker.png");

  // 天球
  txSphere_ = RC::LoadTex("Resources/sky_sphere.png");
  sphere = RC::GenerateSphereEx(txSphere_, 40.0f);
  sphereT_ = RC::GetSphereTransformPtr(sphere);
  RC::SetSphereColor(sphere, {0.6f, 1.0f, 1.0f, 1.0f});

  txball = RC::LoadTex("Resources/monsterBall.png");
  ball = RC::GenerateSphereEx(txball, 1.0f, 16, 16, false);
  ballT_ = RC::GetSphereTransformPtr(ball);
  RC::SetSphereColor(ball, {1.0f, 1.0f, 1.0f, 1.0f});
  ballT_->rotation.y = -1.6f;

  // =============================
  // Sprite初期化
  // =============================

  sprite = RC::LoadSprite("Resources/uvChecker.png", ctx);
}

void SampleScene::OnExit(SceneContext &) {

  RC::UnloadModel(plane);
  plane = -1;
  RC::UnloadModel(model);
  model = -1;
  RC::UnloadModel(blockModel);
  blockModel = -1;

  RC::UnloadSphere(sphere);
  sphere = -1;

  RC::UnloadSprite(sprite);
  sprite = -1;

  RC::UnloadModel(ball);
  ball = -1;

  light = -1;
}

void SampleScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ===========================================
  // ImGui
  // ===========================================

#ifdef _DEBUG

  DrawImGui();

  camera_.DrawImGui();

#endif // _DEBUG

  // ===========================================
  // 更新処理
  // ===========================================

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

  RC::DrawSphere(ball);

  // モデルの描画
  RC::DrawModel(plane);

  RC::DrawModel(model, tx_model);

  //// ===== マップ描画 =====
  //// === マップ上の1だけをインスタンス化してDrawBatch ===
  //std::vector<Transform> instances;
  //instances.reserve(mapWidth * mapHeight);

  //// 1マスの幅（キューブが -0.5〜+0.5 のサイズなら 1.0f でちょうど）
  //const float cell = 1.0f;

  //// マップを画面中央に寄せたい場合のオフセット
  //const float offsetX = -(mapWidth - 1) * 0.5f * cell;
  //const float offsetZ = -(mapHeight - 1) * 0.5f * cell;

  //for (int z = 0; z < mapHeight; ++z) {
  //  for (int x = 0; x < mapWidth; ++x) {
  //    if (map[z][x] != 1)
  //      continue;

  //    Transform t{};
  //    t.scale = {1.0f, 1.0f, 1.0f};
  //    t.rotation = {0.0f, 0.0f, 0.0f};
  //    t.translation = {offsetX + x * cell, -1.0f, offsetZ + z * cell};
  //    instances.push_back(t);
  //  }
  //}

  //RC::DrawModelBatch(blockModel, instances);

  // ===========================================
  // 2D描画
  // ===========================================
  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(sprite);

  //RC::DrawLine({100, 100}, {400, 300}, 1.0f, {1, 0, 0, 1});
  //RC::DrawBox({50, 50}, {250, 180}, false, 4.0f, {0, 1, 0, 1});
  //RC::DrawCircle({600, 200}, 80.0f, false, 5.0f, {0, 0.7f, 1, 1});
}

void SampleScene::DrawImGui() {
  ImGui::Begin("Debug");
  // -------------------
  // Light
  // -------------------
  RC::DrawImGuiLight(light, "light");

  if (ImGui::BeginTabBar("MainDebugTabBar")) {
    // -------------------
    // ModelTab
    // -------------------
    if (ImGui::BeginTabItem("ModelTab")) {

      RC::DrawImGui3D(plane, "plane");

      RC::DrawImGui3D(model, "model");

      RC::DrawSphereImGui(sphere, "skyDome");

      RC::DrawSphereImGui(ball, "ball");

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
    ImGui::EndTabBar();
  }

  ImGui::End();
}
