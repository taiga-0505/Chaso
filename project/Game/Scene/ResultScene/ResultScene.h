#pragma once
#include "Scene.h"
#include <dinput.h>
#include <RC.h>

class ResultScene final : public Scene {
public:
  const char *Name() const override { return "Result"; }
  void OnEnter(SceneContext &) override {}
  void OnExit(SceneContext &) override {}
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
};
