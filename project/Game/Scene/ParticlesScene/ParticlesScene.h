#pragma once
#include "Scene.h"
#include <RC.h>
#include <dinput.h>

class ParticlesScene final : public Scene {
public:
  const char *Name() const override { return "Particles"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &ctx) override;
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
};
