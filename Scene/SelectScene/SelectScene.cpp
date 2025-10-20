#include "SelectScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"
#include "RenderCommon.h"

void SelectScene::Update(SceneManager &sm, SceneContext &ctx) {
}

void SelectScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);
}
