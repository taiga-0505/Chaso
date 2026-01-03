#include "RainParticle.h"
#include <algorithm> // std::clamp 用（使わなければいらない）

namespace RC {

void RainParticle::Initialize(SceneContext &ctx) {

  Particle::Initialize(ctx);

  Emitter &e = EmitterRef();

  e.count = 30;

  e.frequency = 0.05f;
}

// 1個分の初期化
void RainParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                    const Vector3 &emitterPos) {

  // エミッタの「上空の広い範囲」から降ってくるイメージ
  std::uniform_real_distribution<float> distXZ(-10.0f, 10.0f);
  std::uniform_real_distribution<float> distY(8.0f, 12.0f);

  // スケール
  std::uniform_real_distribution<float> distScaleX(0.03f, 0.06f);
  std::uniform_real_distribution<float> distScaleY(0.8f, 1.4f);

  float sx = distScaleX(rng);
  float sy = distScaleY(rng);

  p.transform.scale = {sx, sy, 1.0f};
  p.transform.rotation = {0.0f, 0.0f, 0.0f};

  // 出現位置
  p.transform.translation = {
      emitterPos.x + distXZ(rng),
      emitterPos.y + distY(rng),
      emitterPos.z + distXZ(rng),
  };

  // 速度
  std::uniform_real_distribution<float> distVelXZ(-0.02f, 0.02f);
  std::uniform_real_distribution<float> distVelY(-0.6f, -0.35f);

  p.velocity = {
      distVelXZ(rng), // 横ブレ
      distVelY(rng),  // 下方向（マイナスY）
      distVelXZ(rng),
  };

  // 色はうっすら青っぽい白
  p.color = {0.7f, 0.8f, 1.0f, 1.0f};

  // 寿命（秒
  std::uniform_real_distribution<float> distLife(0.8f, 1.5f);
  p.lifeTime = distLife(rng);
  p.currentTime = 0.0f;
}

// 1個分の更新
void RainParticle::UpdateOneParticle(ParticleData &p, float dt) {

  Particle::UpdateOneParticle(p, dt);

  // 雨粒はクルクル回らせたくないので回転はリセット
  p.transform.rotation = {0.0f, 0.0f, 0.0f};

  // 雨っぽく「重力でだんだん加速」する感じを足す
  const float gravity = -0.02f;
  p.velocity.y += gravity;
}
} // namespace RC
