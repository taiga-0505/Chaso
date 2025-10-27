#include "GameOverScene.h"
#include "Scene/GameScene/GameScene.h"
#include "Input/Input.h"
#include "SceneManager.h"
#include "imgui/imgui.h"
#include "Dx12Core.h"
#include "RenderCommon.h"

void GameOverScene::OnEnter(SceneContext&) { selected_ = 0; }
void GameOverScene::OnExit(SceneContext&) {}

void GameOverScene::Update(SceneManager& sm, SceneContext& ctx) {
    if (!ctx.input)
        return;
    auto* input = ctx.input;
}

void GameOverScene::Render(SceneContext& ctx, ID3D12GraphicsCommandList* cl) {

    // ===========================================
    // 3D描画
    // ===========================================
    RC::PreDraw3D(ctx, cl);

    // ===========================================
    // 2D描画
    // ===========================================
    RC::PreDraw2D(ctx, cl);

}
