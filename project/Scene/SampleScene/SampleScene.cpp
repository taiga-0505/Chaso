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
  // モデル初期化
  // =============================

  plane = RC::LoadModel("Resources/model/plane");
  planeTransform_ = RC::GetModelTransformPtr(plane);

  blockModel = RC::LoadModel("Resources/model/cube");

  model = RC::LoadModel("Resources/model/teapot");
  tx_model = RC::LoadTex("Resources/uvChecker.png");

  // 天球
  txSphere_ = RC::LoadTex("Resources/sky_sphere.png", false);
  sphere = RC::GenerateSphereEx(txSphere_, 40.0f);
  sphereT_ = RC::GetSphereTransformPtr(sphere);
  RC::SetSphereColor(sphere, {0.6f, 1.0f, 1.0f, 1.0f});

  // =============================
  // Sprite初期化
  // =============================

  sprite = RC::LoadSprite("Resources/uvChecker.png", ctx);

  // =============================
  // Sound初期化
  // =============================

  sound = new RC::Sound();
  sound->Initialize("Resources/Sounds/Alarm01.wav");
}

void SampleScene::OnExit(SceneContext &) {

  RC::UnloadModel(plane);
  plane = -1;
  RC::UnloadModel(model);
  model = -1;
  RC::UnloadModel(blockModel);
  blockModel = -1;

  if (sound) {
    delete sound;
    sound = nullptr;
  }

  RC::UnloadSphere(sphere);
  sphere = -1;
}

void SampleScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ===========================================
  // ImGui
  // ===========================================

#ifdef _DEBUG

  RC::DrawImGui3D(plane, "plane");

  RC::DrawImGui3D(model, "model");

  RC::DrawImGui2D(sprite, "sprite");

  RC::DrawSphereImGui(sphere, "skyDome");

  sound->SoundImGui("Alarm01");

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

  RC::SetCamera(view_, proj_);

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

  // モデルの描画
  RC::DrawModel(plane);

  RC::DrawModel(model, tx_model);

  // ===== マップ描画 =====
  // === マップ上の1だけをインスタンス化してDrawBatch ===
  std::vector<Transform> instances;
  instances.reserve(mapWidth * mapHeight);

  // 1マスの幅（キューブが -0.5〜+0.5 のサイズなら 1.0f でちょうど）
  const float cell = 1.0f;

  // マップを画面中央に寄せたい場合のオフセット
  const float offsetX = -(mapWidth - 1) * 0.5f * cell;
  const float offsetZ = -(mapHeight - 1) * 0.5f * cell;

  for (int z = 0; z < mapHeight; ++z) {
    for (int x = 0; x < mapWidth; ++x) {
      if (map[z][x] != 1)
        continue;

      Transform t{};
      t.scale = {1.0f, 1.0f, 1.0f};
      t.rotation = {0.0f, 0.0f, 0.0f};
      t.translation = {offsetX + x * cell, -1.0f, offsetZ + z * cell};
      instances.push_back(t);
    }
  }

  RC::DrawModelBatch(blockModel, instances);

  // ===========================================
  // 2D描画
  // ===========================================
  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(sprite);
}
