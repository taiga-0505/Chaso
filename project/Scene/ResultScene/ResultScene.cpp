#include "ResultScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"
#include "RenderCommon.h"

void ResultScene::Update(SceneManager &sm, SceneContext &ctx) {
}

void ResultScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);
}
