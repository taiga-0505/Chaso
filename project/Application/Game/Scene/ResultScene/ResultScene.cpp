#include "ResultScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"

ResultScene::~ResultScene() {
  SceneContext dummy{};
  OnExit(dummy);
}
#include "imgui/imgui.h"

void ResultScene::OnEnter(SceneContext &ctx) {
  // ======= カメラ初期化 =======
  // 視錐台パラメータ
  const float kNearZ = 0.1f;
  const float kFarZ = 100.0f;
  camera_.Initialize(ctx.input, {0, 5, -20}, {0, 0, 0}, 0.45f,
                     float(ctx.app->width) / ctx.app->height, kNearZ, kFarZ);

  // ======= スカイドーム生成 =======
  txSphere_ = RC::LoadTex("Resources/skydome.jpg");
  const float kSkyRadius = kFarZ * 0.95f;
  skydomeModel = RC::GenerateSphereEx(txSphere_, kSkyRadius);
  sphereT_ = RC::GetSphereTransformPtr(skydomeModel);
  RC::SetSphereColor(skydomeModel, {0.6f, 1.0f, 1.0f, 1.0f});

  clearSprite = RC::LoadSprite("Resources/UI/Clear.png", ctx);

  RC::SetSpriteScreenSize(clearSprite, 1280, 720);
}

void ResultScene::OnExit(SceneContext &ctx) {}

void ResultScene::Update(SceneManager &sm, SceneContext &ctx) {

  // ======= カメラ更新 =======
  // 固定デルタタイム
  const float dt = 1.0f / 60.0f;
  camera_.Update(dt);

  // ======= ビュー・プロジェクション更新 =======
  view_ = camera_.GetView();
  proj_ = camera_.GetProjection();
  RC::SetCamera(view_, proj_, camera_.GetWorldPos());

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
    sm.RequestChange("Title");
  }
}

void ResultScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::DrawSphere(skydomeModel);

  RC::PreDraw2D(ctx, cl);

  RC::DrawSprite(clearSprite);

}
