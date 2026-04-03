#include "FlashParticle.h"
#include <Math/Math.h>
#include <random>
#include <numbers>

namespace RC {

void FlashParticle::Initialize(SceneContext &ctx) {
  Particle::Initialize(ctx);

  Emitter &e = EmitterRef();
  e.count = 8;        // 資料に基づき 8個
  e.frequency = 0.05f; // 放出頻度
  e.globalScale = {1.0f, 1.0f, 1.0f};
  e.globalColor = {1.0f, 1.0f, 1.0f, 1.0f};
  e.timeScale = 1.0f;

  SetEmitterAutoSpawn(true);
}

void FlashParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                    const Vector3 &emitterPos) {
  std::uniform_real_distribution<float> distScaleY(scaleYMin_, scaleYMax_);
  std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
  std::uniform_real_distribution<float> distSpeed(speedMin_, speedMax_);
  std::uniform_real_distribution<float> distLife(lifeMin_, lifeMax_);
  std::uniform_real_distribution<float> distDir(-1.0f, 1.0f);

  // 資料(Reference)に基づく細長いスケールと回転
  // スケール: { 0.05f, random_scale, 1.0f }
  p.transform.scale = {0.05f, distScaleY(rng), 1.0f};

  // 回転: { 0.0f, 0.0f, random_rot }
  p.transform.rotation = {0.0f, 0.0f, distRotate(rng)};

  // 位置はエミッタの位置
  p.transform.translation = emitterPos;

  // 速度: その場に留まる（資料に基づき拡散しない）
  p.velocity = {0, 0, 0};

  p.color = {1.0f, 1.0f, 1.0f, 1.0f}; // 基本は白（Emitterの色で一括制御可能）
  p.lifeTime = distLife(rng);
  p.currentTime = 0.0f;
}

void FlashParticle::UpdateOneParticle(ParticleData &p, float dt) {
  p.currentTime += dt;
}

} // namespace RC
