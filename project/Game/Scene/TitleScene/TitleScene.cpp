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
}

void TitleScene::OnExit(SceneContext &ctx) {

}

void TitleScene::Update(SceneManager &sm, SceneContext &ctx) {

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Game");
  }
}

void TitleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {

  RC::PreDraw3D(ctx, cl);


  RC::PreDraw2D(ctx, cl);
}
