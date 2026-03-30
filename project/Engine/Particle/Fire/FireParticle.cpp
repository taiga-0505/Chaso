#include "FireParticle.h"
#include <algorithm>

namespace RC {

void FireParticle::Initialize(SceneContext &ctx) {
  Particle::Initialize(ctx);

  Emitter &e = EmitterRef();
  e.count = 100;
  e.frequency = 1.0f;
}

std::list<ParticleData> FireParticle::Emit(const Emitter &emitter,
                                           std::mt19937 &randomEngine) {
  Emitter &e = EmitterRef();

  size_t current = particles.size();
  if (current == 0) {
    e.count = 100;
  } else if (current < 30) {
    e.count = 70;
  } else if (current < 50) {
    e.count = 50;
  } else {
    e.count = 10;
  }

  std::list<ParticleData> newParticles;
  for (uint32_t i = 0; i < e.count; ++i) {
    newParticles.push_back(
        MakeNewParticle(randomEngine, e.transform.translation));
  }
  return newParticles;
}

void FireParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                    const Vector3 &emitterPos) {

  // エミッタの周りにちょっとだけ広がる
  std::uniform_real_distribution<float> distXZ(-0.3f, 0.3f);
  std::uniform_real_distribution<float> distY(0.0f, 0.2f);

  // 大きさ・寿命・速度用
  std::uniform_real_distribution<float> distScale(0.3f, 0.8f);
  std::uniform_real_distribution<float> distVelXZ(-0.01f, 0.01f);
  std::uniform_real_distribution<float> distVelY(0.04f, 0.10f);
  std::uniform_real_distribution<float> distLife(0.5f, 1.2f);

  float s = distScale(rng);

  // 位置・スケール・回転
  p.transform.scale = {s, s, s};
  p.transform.rotation = {0.0f, 0.0f, 0.0f};
  p.transform.translation = {
      emitterPos.x + distXZ(rng),
      emitterPos.y + distY(rng),
      emitterPos.z + distXZ(rng),
  };

  // ちょっとだけランダムにゆらぎながら上に上がる
  p.velocity = {
      distVelXZ(rng),
      distVelY(rng), // 上向き（+Y 側）
      distVelXZ(rng),
  };

  // 最初は明るいオレンジ
  p.color = {1.0f, 0.75f, 0.3f, 1.0f};

  // 寿命は短め
  p.lifeTime = distLife(rng);
  p.currentTime = 0.0f;
}

// 1個分の更新
void FireParticle::UpdateOneParticle(ParticleData &p, float dt) {
  // まずは基盤クラスの標準挙動で
  //  - 位置 += velocity
  //  - currentTime += dt
  //  - Z回転
  //  - 加速度フィールドの影響
  Particle::UpdateOneParticle(p, dt);

  // 0.0 ～ 1.0 の経過率
  float t = 0.0f;
  if (p.lifeTime > 0.0f) {
    t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);
  }

  // だんだん小さくしていく（指数的にしぼむ感じ）
  // shrinkRate を変えると収縮の速さが変わる
  float shrinkRate = 1.5f; // 大きいほど早く小さくなる
  float shrink = (std::max)(0.0f, 1.0f - shrinkRate * dt);
  p.transform.scale.x *= shrink;
  p.transform.scale.y *= shrink;
  p.transform.scale.z *= shrink;

  // 色をオレンジ → 赤黒い色へ補間
  const float startR = 1.0f, startG = 0.75f, startB = 0.3f;
  const float endR = 0.3f, endG = 0.05f, endB = 0.0f;

  p.color.x = startR * (1.0f - t) + endR * t;
  p.color.y = startG * (1.0f - t) + endG * t;
  p.color.z = startB * (1.0f - t) + endB * t;
}

} // namespace RC
