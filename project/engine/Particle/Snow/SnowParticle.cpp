#include "SnowParticle.h"
#include <algorithm> // std::clamp 用

namespace RC {

void SnowParticle::Initialize(SceneContext &ctx) {
  // 共通部分は基底クラスに任せる
  Particle::Initialize(ctx);

  // 雪用エミッタ設定
  Emitter &e = EmitterRef();

  e.count = 15;

  e.frequency = 0.12f;
}

// 1個分の初期化
void SnowParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                    const Vector3 &emitterPos) {

  // エミッタの「ちょっと上の広い範囲」からふわっと落ちるイメージ
  std::uniform_real_distribution<float> distXZ(-8.0f, 8.0f);
  std::uniform_real_distribution<float> distY(4.0f, 8.0f);

  // 大きさ・速度・寿命
  std::uniform_real_distribution<float> distScale(0.15f, 0.35f);
  std::uniform_real_distribution<float> distVelXZ(-0.02f, 0.02f);
  std::uniform_real_distribution<float> distVelY(-0.003f, -0.001f);
  std::uniform_real_distribution<float> distLife(2.5f, 5.0f);

  float s = distScale(rng);

  // 位置・スケール・回転
  p.transform.scale = {s, s, s};
  p.transform.rotation = {0.0f, 0.0f, 0.0f};
  p.transform.translation = {
      emitterPos.x + distXZ(rng),
      emitterPos.y + distY(rng),
      emitterPos.z + distXZ(rng),
  };

  // ゆっくり落ちる＋ちょっとだけ横方向にランダム
  p.velocity = {
      distVelXZ(rng),
      distVelY(rng), // 下方向（マイナスY）
      distVelXZ(rng),
  };

  // ほんのり青みのある白
  p.color = {0.95f, 0.98f, 1.0f, 1.0f};
  // 寿命
  p.lifeTime = distLife(rng);
  p.currentTime = 0.0f;
}

// 1個分の更新
void SnowParticle::UpdateOneParticle(ParticleData &p, float dt) {

  Particle::UpdateOneParticle(p, dt);

  // 重力っぽく少しずつ加速させる
  const float gravity = -0.001f;
  p.velocity.y += gravity;
  // 落下速度が速くなりすぎないようにクランプ
  p.velocity.y = std::clamp(p.velocity.y, -0.18f, -0.03f);

  // 横方向はちょっとずつ減衰させて、全体的にふわっと
  p.velocity.x *= 0.90f;
  p.velocity.z *= 0.90f;
}

} // namespace RC
