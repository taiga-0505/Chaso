#pragma once
#include "Camera/CameraController.h"
#include "Scene.h"

class GameScene final : public Scene {
public:
  const char *Name() const override { return "Game"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &) override;

  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
  Matrix4x4 view_, proj_;
  // カメラ
  CameraController camera_;
};
