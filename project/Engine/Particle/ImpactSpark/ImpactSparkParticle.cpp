#include "ImpactSparkParticle.h"
#include <Math/Math.h>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

// ===== ローカルユーティリティ =====

// 3次元ベクトルの長さ二乗
static inline float LenSq3_(const RC::Vector3 &v) {
  return v.x * v.x + v.y * v.y + v.z * v.z;
}

// 3次元ベクトルの内積
static inline float Dot3(const RC::Vector3 &a, const RC::Vector3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

// 3次元ベクトルの長さ二乗
static inline float LenSq3(const RC::Vector3 &v) {
  return v.x * v.x + v.y * v.y + v.z * v.z;
}

// 法線と平行になりにくい任意軸を返す
static inline RC::Vector3 SafeOrtho(const RC::Vector3 &n) {
  if (std::fabs(n.y) > 0.9f)
    return {1.0f, 0.0f, 0.0f};
  return {0.0f, 1.0f, 0.0f};
}

// 線形補間
static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// 法線を中心とするコーン内でランダムな方向を取得
static RC::Vector3 RandomDirInCone(std::mt19937 &rng, const RC::Vector3 &normal,
                                   float coneRad) {
  // 正規直交基底 (t, b, n) を作る
  RC::Vector3 n = Normalize(normal);
  RC::Vector3 t = Normalize(Cross(SafeOrtho(n), n));
  RC::Vector3 b = Normalize(Cross(n, t));

  // コーン内をサンプル
  std::uniform_real_distribution<float> u01(0.0f, 1.0f);
  float u = u01(rng);
  float v = u01(rng);

  float cosMax = std::cos(coneRad);
  float cosA = Lerp(cosMax, 1.0f, u); // [cos(cone), 1]
  float sinA = std::sqrt((std::max)(0.0f, 1.0f - cosA * cosA));
  float phi = 2.0f * std::numbers::pi_v<float> * v;

  RC::Vector3 dir =
      Add(Multiply(n, cosA), Add(Multiply(t, sinA * std::cos(phi)),
                                 Multiply(b, sinA * std::sin(phi))));

  return Normalize(dir);
}

// 円盤内のランダムなオフセットを取得（軸は円盤の法線）
static RC::Vector3 RandomOffsetInDisc(std::mt19937 &rng,
                                      const RC::Vector3 &axis, float radius) {
  if (radius <= 0.0f) {
    return {0.0f, 0.0f, 0.0f};
  }

  RC::Vector3 n = axis;
  if (LenSq3(n) <= 1e-8f) {
    n = {0.0f, 1.0f, 0.0f};
  }
  n = Normalize(n);

  // 正規直交基底(t,b,n)
  RC::Vector3 t = Normalize(Cross(SafeOrtho(n), n));
  RC::Vector3 b = Normalize(Cross(n, t));

  std::uniform_real_distribution<float> u01(0.0f, 1.0f);
  float u = u01(rng);
  float v = u01(rng);

  // 円盤内を一様にするなら sqrt(u)
  float r = std::sqrt(u) * radius;
  float theta = 2.0f * std::numbers::pi_v<float> * v;

  return Add(Multiply(t, r * std::cos(theta)),
             Multiply(b, r * std::sin(theta)));
}

} // namespace

namespace RC {

void ImpactSparkParticle::Initialize(SceneContext &ctx) {
  Particle::Initialize(ctx);
  SetEmitterAutoSpawn(false); // 明示的に Burst / SetImpact で出す
  ClearAll();
}

void ImpactSparkParticle::TrimToMax_() {
  // 最大数を超えた分だけ古い粒子を破棄
  while ((int)particles.size() > maxParticles_) {
    particles.erase(particles.begin());
  }
}

void ImpactSparkParticle::Burst(const Vector3 &pos, const Vector3 &normal,
                                int count) {
  if (count <= 0)
    return;

  Vector3 basePos = pos;

  // 着弾面にめり込みにくいよう、法線方向に少し浮かせる
  basePos = Add(basePos, Multiply(burstNormal_, surfaceBias_));

  for (int i = 0; i < count; ++i) {
    ParticleData p{};

    // レーザー幅に応じてスポーン位置を散らす
    Vector3 spawnPos = basePos;
    if (impactRadius_ > 0.0f) {
      spawnPos =
          Add(spawnPos, RandomOffsetInDisc(rng_, impactAxis_, impactRadius_));
    }

    InitParticleCore(p, rng_, spawnPos);
    particles.push_back(p);
  }
  TrimToMax_();
}

void ImpactSparkParticle::StopImpact() {
  // 連続放出を停止し、次回開始をバースト扱いにする
  emitting_ = false;
  emitTimer_ = 0.0f;
}

void ImpactSparkParticle::SetImpactFromBeam(const Vector3 &start,
                                            const Vector3 &end, float beamWidth,
                                            const ImpactEmitDesc &desc) {
  Vector3 dir = {end.x - start.x, end.y - start.y, end.z - start.z};
  if (LenSq3_(dir) <= 1e-8f) {
    return;
  }

  Vector3 beamDir = Normalize(dir);

  // 円盤の向きと半径を保存（Burst がこれを使って散らす）
  impactAxis_ = beamDir;
  impactRadius_ = (std::max)(0.0f, beamWidth * impactRadiusFactor_);

  // 法線はレーザーの逆向きで近似
  Vector3 hitNormal = Multiply(beamDir, -1.0f);
  SetImpact(end, hitNormal, desc);
}

void ImpactSparkParticle::SetImpactFromBeam(const Vector3 &start,
                                            const Vector3 &end,
                                            const ImpactEmitDesc &desc) {
  Vector3 dir = {end.x - start.x, end.y - start.y, end.z - start.z};
  if (LenSq3_(dir) <= 1e-8f) {
    // 長さゼロなら何もしない（止めたいなら外で StopImpact 呼んでね）
    return;
  }

  Vector3 beamDir = Normalize(dir);

  // 法線が取れない場合の簡易法線：レーザーの逆向き
  Vector3 hitNormal = Multiply(beamDir, -1.0f);

  SetImpact(end, hitNormal, desc);
}

void ImpactSparkParticle::SetImpact(const Vector3 &hitPos,
                                    const Vector3 &hitNormal,
                                    const ImpactEmitDesc &desc) {
  emitDesc_ = desc;

  // まず開始判定（最初の1回だけドカッと）
  if (!emitting_) {
    emitting_ = true;
    emitTimer_ = 0.0f; // すぐ出したい

    if (emitDesc_.burstOnStart > 0) {
      Burst(hitPos, hitNormal, emitDesc_.burstOnStart);
    }
  }

  // 以降は interval ごとにチリチリ
  const float dt = GetDeltaTime(); // Particle基盤の固定Δt(1/60)
  emitTimer_ -= dt;

  if (emitDesc_.interval <= 0.0f) {
    // 毎フレ出す
    Burst(hitPos, hitNormal, emitDesc_.countPerTick);
    return;
  }

  while (emitTimer_ <= 0.0f) {
    Burst(hitPos, hitNormal, emitDesc_.countPerTick);
    emitTimer_ += emitDesc_.interval;
  }
}

void ImpactSparkParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                           const Vector3 &emitterPos) {
  std::uniform_real_distribution<float> u01(0.0f, 1.0f);

  // 寿命
  float life = Lerp(lifeMin_, lifeMax_, u01(rng));
  p.lifeTime = (std::max)(life, 1e-3f);
  p.currentTime = 0.0f;

  // コーン方向に飛ばす
  float coneRad = (coneAngleDeg_ / 180.0f) * std::numbers::pi_v<float>;
  Vector3 dir = RandomDirInCone(rng, burstNormal_, coneRad);

  // 速度
  float speed = Lerp(speedMin_, speedMax_, u01(rng));
  p.velocity = Multiply(dir, speed);

  // 位置
  p.transform.translation = emitterPos;

  // 火花の見た目（速度方向に伸ばす）
  float len = Lerp(streakMin_, streakMax_, u01(rng));
  p.transform.scale = {thickness_ * 0.5f, len * 0.5f, 1.0f};

  // 回転は使わないので「元の長さ」を保存（縮ませる用）
  p.transform.rotation = {len, 0.0f, 0.0f};

  // 色
  p.color = startColor_;
}

void ImpactSparkParticle::UpdateOneParticle(ParticleData &p, float dt) {
  // move（このParticle基盤は velocity を「毎フレ加算」方式）
  p.transform.translation = Add(p.transform.translation, p.velocity);

  // 速度に重力 + 減衰
  p.velocity = Add(p.velocity, Multiply(Vector3{0.0f, -gravity_, 0.0f}, dt));
  p.velocity = Multiply(p.velocity, damping_);

  // 時間
  p.currentTime += dt;

  // 火花を寿命に合わせて短くしていく
  float t =
      std::clamp(p.currentTime / (std::max)(p.lifeTime, 1e-3f), 0.0f, 1.0f);
  float baseLen = p.transform.rotation.x; // 保存してたやつ
  float nowLen = baseLen * (1.0f - t * 0.85f);
  nowLen = (std::max)(nowLen, thickness_ * 0.8f);
  p.transform.scale.y = nowLen * 0.5f;

  // 色を白→オレンジに寄せる
  p.color.x = Lerp(startColor_.x, endColor_.x, t);
  p.color.y = Lerp(startColor_.y, endColor_.y, t);
  p.color.z = Lerp(startColor_.z, endColor_.z, t);
}

Matrix4x4
ImpactSparkParticle::BuildWorldMatrix(const ParticleData &p,
                                      const Matrix4x4 &billboardMatrix) const {
  // 「速度方向を上(up)」として、カメラに正面向ける軸ビルボード
  Vector3 up = p.velocity;
  if (LenSq3(up) <= 1e-10f)
    up = {0, 1, 0};
  up = Normalize(up);

  // ビルボード行列からカメラ方向を取り出す
  Vector3 viewDir = Normalize({billboardMatrix.m[2][0], billboardMatrix.m[2][1],
                               billboardMatrix.m[2][2]});

  // right = viewDir x up
  Vector3 right = Cross(viewDir, up);
  if (LenSq3(right) <= 1e-8f) {
    Vector3 any = SafeOrtho(up);
    right = Cross(any, up);
  }
  right = Normalize(right);

  // forward = up x right
  Vector3 forward = Normalize(Cross(up, right));

  // 回転行列を構成
  Matrix4x4 rot = MakeIdentity4x4();
  rot.m[0][0] = right.x;
  rot.m[0][1] = right.y;
  rot.m[0][2] = right.z;
  rot.m[1][0] = up.x;
  rot.m[1][1] = up.y;
  rot.m[1][2] = up.z;
  rot.m[2][0] = forward.x;
  rot.m[2][1] = forward.y;
  rot.m[2][2] = forward.z;

  Matrix4x4 scaleMat = MakeScaleMatrix(p.transform.scale);
  Matrix4x4 transMat = MakeTranslateMatrix(p.transform.translation);

  // scale -> rotate -> translate
  return Multiply(Multiply(scaleMat, rot), transMat);
}

float ImpactSparkParticle::ComputeAlpha(const ParticleData &p) const {
  if (p.lifeTime <= 0.0f)
    return 0.0f;
  float t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);

  // ふわっと消える（序盤は明るく残す）
  float a = 1.0f - t;
  return a * a;
}

} // namespace RC
