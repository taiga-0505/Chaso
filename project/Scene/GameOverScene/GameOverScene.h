#pragma once
#include "Scene.h"
#include <dinput.h>

class GameOverScene final : public Scene {
public:
	const char* Name() const override { return "GameOver"; }
	void OnEnter(SceneContext& ctx) override;
	void OnExit(SceneContext&) override;
	void Update(SceneManager& sm, SceneContext& ctx) override;
	void Render(SceneContext& ctx, ID3D12GraphicsCommandList* cl) override;

private:
	int selected_ = 0; // 0=Retry, 1=Select
};
