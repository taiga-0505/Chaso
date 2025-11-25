#include "ParticlesScene.h"
#include "Input/Input.h"
#include "RenderCommon.h"
#include "SceneManager.h"
#include "imgui/imgui.h"

void ParticlesScene::OnEnter(SceneContext &ctx) {
}

void ParticlesScene::OnExit(SceneContext &ctx) {
}

void ParticlesScene::Update(SceneManager &sm, SceneContext &ctx) {
  
}

void ParticlesScene::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  ctx.core->Clear(0.05f, 0.05f, 0.05f, 1.0f);
  RC::PreDraw3D(ctx, cl);

  RC::PreDraw2D(ctx, cl);
}
