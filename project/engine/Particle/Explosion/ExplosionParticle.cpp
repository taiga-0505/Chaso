#include "ExplosionParticle.h"
#include <Math/Math.h> // Add, Multiply など
#include <algorithm>   // std::clamp
#include <cmath>       // std::sin, std::cos
#include <numbers>     // std::numbers::pi_v

namespace RC {

const char *ExplosionParticle::GetTexturePath() const {
  // 爆発用のテクスチャを用意したらここを差し替える
  // 例：Resources/Particle/explosion.png
  return "Resources/Particle/circle.png";
}

void ExplosionParticle::Trigger(const Vector3 &position) {
  // 一発きりの爆発なので自動スポーンは切る
  SetEmitterAutoSpawn(false);

  // エミッタ位置をセット
  auto &emitter = EmitterRef();
  emitter.transform.translation = position;

  // 一度に出すパーティクルの数（kNumMaxInstance=100 以内にする）
  emitter.count = 80;

  // 既存のパーティクルを消してから、爆発分を生成
  ClearAll();

  AddParticlesFromEmitter();
}


// 1個分のパーティクル初期化
void ExplosionParticle::InitParticleCore(ParticleData &particle,
                                         std::mt19937 &randomEngine,
                                         const Vector3 &emitterTranslate) {
  // 角度と速度
  std::uniform_real_distribution<float> distAngle(
      0.0f, 2.0f * std::numbers::pi_v<float>);
  std::uniform_real_distribution<float> distPitch(-0.4f,
                                                  0.4f); // 少し上下にばらける
  std::uniform_real_distribution<float> distSpeed(minSpeed_, maxSpeed_);
  std::uniform_real_distribution<float> distLife(minLife_, maxLife_);
  std::uniform_real_distribution<float> distScale(0.15f, 0.35f);

  float yaw = distAngle(randomEngine);   // Y軸周り
  float pitch = distPitch(randomEngine); // 上下方向
  float speed = distSpeed(randomEngine);

  // 放射状に飛ぶ向き
  Vector3 dir{
      std::cos(pitch) * std::cos(yaw),
      std::sin(pitch),
      std::cos(pitch) * std::sin(yaw),
  };
  // Normalize(dir) があるなら正規化してもOK

  // 位置：エミッタの周辺に少しだけランダムオフセット
  std::uniform_real_distribution<float> distPos(-0.1f, 0.1f);
  particle.transform.translation = {
      emitterTranslate.x + distPos(randomEngine),
      emitterTranslate.y + distPos(randomEngine),
      emitterTranslate.z + distPos(randomEngine),
  };

  float s = distScale(randomEngine);
  particle.transform.scale = {s, s, s};
  particle.transform.rotation = {0.0f, 0.0f, 0.0f};

  // 速度
  particle.velocity = Multiply(dir, speed);

  // 開始時の色（黄色寄り）
  particle.color = {1.0f, 0.8f, 0.3f, 1.0f};

  particle.lifeTime = distLife(randomEngine);
  particle.currentTime = 0.0f;
}

// 1個分のパーティクル更新
void ExplosionParticle::UpdateOneParticle(ParticleData &p, float dt) {
  // 位置更新
  p.transform.translation = Add(p.transform.translation, p.velocity);
  p.currentTime += dt;

  // 速度をだんだん減らす（空気抵抗）
  p.velocity = Multiply(p.velocity, 0.9f);

  // 少しだけ上方向に持ち上げる（煙が上がるイメージ）
  Vector3 up{0.0f, 1.0f, 0.0f};
  p.velocity = Add(p.velocity, Multiply(up, 0.03f));

  // スケール：時間とともに大きく
  float t = p.currentTime / p.lifeTime;
  t = std::clamp(t, 0.0f, 1.0f);

  float startScale = 0.2f;
  float endScale = 1.8f;
  float s = startScale + (endScale - startScale) * t;
  p.transform.scale = {s, s, s};

  // Z回転を少しだけ
  p.transform.rotation.z += 5.0f * dt;

  // 色：黄色 → オレンジ → 灰色に変化
  auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

  Vector3 colStart{1.0f, 0.9f, 0.5f}; // 明るい黄
  Vector3 colMid{1.0f, 0.5f, 0.0f};   // オレンジ
  Vector3 colEnd{0.2f, 0.2f, 0.2f};   // 灰色

  Vector3 c{};
  if (t < 0.5f) {
    float k = t / 0.5f;
    c.x = lerp(colStart.x, colMid.x, k);
    c.y = lerp(colStart.y, colMid.y, k);
    c.z = lerp(colStart.z, colMid.z, k);
  } else {
    float k = (t - 0.5f) / 0.5f;
    c.x = lerp(colMid.x, colEnd.x, k);
    c.y = lerp(colMid.y, colEnd.y, k);
    c.z = lerp(colMid.z, colEnd.z, k);
  }

  // αはベースクラス側で currentTime / lifeTime から決めてくれるので、
  // ここでは RGB だけ設定しておく
  p.color = {c.x, c.y, c.z, 1.0f};
}

} // namespace RC
