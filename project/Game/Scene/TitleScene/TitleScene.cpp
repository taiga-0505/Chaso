#include "TitleScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void TitleScene::Update(SceneManager &sm, SceneContext &ctx) {

  if (ctx.input->IsKeyTrigger(DIK_SPACE)) {
    sm.RequestChange("Game");
  }
}

void TitleScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {

  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);
}
