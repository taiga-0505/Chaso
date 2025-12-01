#pragma once
#include "Scene.h"
#include <RC.h>

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

  RC::FireParticle fire_particle_;

  RC::RainParticle rain_particle_;

  RC::SnowParticle snow_particle_;

  RC::CircleParticle circle_particle_;

  RC::ExplosionParticle explosion_particle_;

  bool isParticle = true;
  bool isFire = false;
  bool isRain = false;
  bool isSnow = false;
  bool isCircle = false;
  bool isExplosion = false;

  int guide;
};
