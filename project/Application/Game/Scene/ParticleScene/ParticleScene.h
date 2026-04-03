#pragma once
#include "Scene.h"
#include <RC.h>
#include "Graphics/Effect/HitEffect.h"
#include <memory>
#include <vector>

class ParticleScene final : public Scene {
public:
  const char *Name() const override { return "Particle"; }
  void OnEnter(SceneContext &ctx) override;
  void OnExit(SceneContext &ctx) override;
  void Update(SceneManager &sm, SceneContext &ctx) override;
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) override;
  ~ParticleScene() override;

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

  RC::LaserParticle laser_particle_;
  RC::Vector3 laserStart_ = {0.0f, -1.0f, 0.0f};
  RC::Vector3 laserEnd_ = {0.0f, 2.0f, 0.0f};
  float laserWidth_ = 0.2f;
  float laserLife_ = 0.05f;
  RC::Vector4 laserColor_ = {0.2f, 0.9f, 1.0f, 1.0f};
  int streakCount_ = 120;
  float streakMinLen_ = 0.15f;
  float streakMaxLen_ = 0.8f;
  float streakWidth_ = 0.03f;
  float streakSpreadZRatio_ = 0.15f;

  RC::ImpactSparkParticle impact_particle_;
  RC::ImpactEmitDesc impactDesc_;

  RC::WindParticle wind_particle_;
  RC::Vector3 windOrigin_ = {0.0f, 0.0f, 0.0f};
  RC::Vector3 windForce_ = {1.0f, 0.0f, 0.0f};
  float windRange_ = 6.0f;
  float windRadius_ = 0.4f;

  RC::HitEffect hit_effect_;

  bool isParticle = true;
  bool isFire = false;
  bool isRain = false;
  bool isSnow = false;
  bool isCircle = false;
  bool isExplosion = false;
  bool isLaser = false;
  bool isWind = false;
  bool isHitEffect = false;

  int guide;
};
