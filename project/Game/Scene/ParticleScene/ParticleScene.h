#pragma once
#include "Scene.h"
#include <RC.h>
#include <dinput.h>

class ParticleScene final : public Scene {
public:
  const char *Name() const override { return "Particle"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &ctx) override;
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;

private:
  RC::Matrix4x4 view_, proj_;

  // カメラ
  RC::CameraController camera_;

  RC::Particle particle_;
};
