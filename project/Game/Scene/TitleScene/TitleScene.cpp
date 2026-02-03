#include "TitleScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void TitleScene::OnEnter(SceneContext &ctx) {

  // ======= カメラ初期化 =======
  // 視錐台パラメータ
  const float kNearZ = 0.1f;
  const float kFarZ = 100.0f;
  camera_.Initialize(ctx.input, {5, 5, -30}, {0, 0, 0}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, kNearZ, kFarZ);

  frameCount = 0;

  // タイトルモデル読み込み
  titleModel = RC::LoadModel("Resources/model/Title/Title.obj");
  RC::SetModelLightingMode(titleModel, None);
  titleT_ = RC::GetModelTransformPtr(titleModel);

  titleT_->translation = {6.0f, 5.0f, 30.0f};
  titleT_->scale = {1.0f, 1.0f, 1.0f}; 

  // ======= スカイドーム生成 =======
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSphereEx(txSphere_, kSkyRadius);
  sphereT_ = RC::GetSphereTransformPtr(skydomeModel);
  RC::SetSphereColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});
}

void TitleScene::OnExit(SceneContext &ctx) {
  // タイトルモデル破棄
  RC::UnloadModel(titleModel);
  titleModel = -1;
}

void TitleScene::Update(SceneManager &sm, SceneContext &ctx) {

    frameCount++;

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  camera_.Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();
  RC::SetCamera(view_, proj_, camera_.GetWorldPos());

  // タイトルモデルをふわふわ動かす
  if (titleT_) {
    const float t = frameCount * 0.02f;
    titleT_->translation.y = 5.0f + std::sin(t) * 0.5f;
  }

   // ======= スカイドーム更新 =======
  if (sphereT_) {
    // カメラ座標に追従
    sphereT_->translation = camera_.GetWorldPos();
    // 高さオフセット
    sphereT_->translation.y -= 10.0f;
    // 自転処理
    sphereT_->rotation.y += 0.0005f;
  }

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Select");
  }
}

void TitleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {

  RC::PreDraw3D(ctx, cl);

  RC::DrawSphere(skydomeModel);

  RC::DrawModel(titleModel);

  RC::PreDraw2D(ctx, cl);
}
