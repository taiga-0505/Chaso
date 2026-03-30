#include "WindParticle.h"
#include <Math/Math.h>
#include <algorithm>
#include <cmath>
#include <random>

namespace {
static inline float LenSq(const RC::Vector3 &v) {
  return v.x * v.x + v.y * v.y + v.z * v.z;
}
static inline float Len(const RC::Vector3 &v) { return std::sqrt(LenSq(v)); }

static inline RC::Vector3 NormalizeSafe(const RC::Vector3 &v) {
  float l = Len(v);
  if (l <= 1e-6f) {
    return {1.0f, 0.0f, 0.0f};
  }
  return {v.x / l, v.y / l, v.z / l};
}

static inline float Smooth01(float t) { return t * t * (3.0f - 2.0f * t); }
static inline float SmoothStep(float e0, float e1, float x) {
  if (std::fabs(e1 - e0) < 1e-6f)
    return (x >= e1) ? 1.0f : 0.0f;
  float t = (x - e0) / (e1 - e0);
  t = std::clamp(t, 0.0f, 1.0f);
  return Smooth01(t);
}

static inline float RandRange(std::mt19937 &rng, float a, float b) {
  std::uniform_real_distribution<float> dist(a, b);
  return dist(rng);
}
} // namespace

namespace RC {

void WindParticle::Initialize(SceneContext &ctx) {
  Particle::Initialize(ctx);

  Emitter &e = EmitterRef();
  e.count = 18;
  e.frequency = 0.03f;

  SetEmitterAutoSpawn(true);
}

void WindParticle::SetActive(bool active) {
  SetEmitterAutoSpawn(active);
  if (!active) {
    ClearAll();
  } else if (particles.empty()) {
    AddParticlesFromEmitter();
  }
}

void WindParticle::SetWind(const Vector3 &origin, const Vector3 &force,
                           float range, float radius, bool active) {
  const float forceLen = Len(force);
  if (forceLen <= 1e-6f || !active) {
    SetActive(false);
    return;
  }

  windDir_ = NormalizeSafe(force);
  windRight_ = NormalizeSafe({-windDir_.y, windDir_.x, 0.0f});

  windRange_ = (std::max)(range, 0.1f);
  windRadius_ = (std::max)(radius, 0.02f);

  windSpeed_ = std::clamp(forceLen * speedScale_, 0.03f, 0.6f);

  EmitterRef().transform.translation = origin;
  SetActive(true);
}

void WindParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                    const Vector3 &emitterPos) {
  const float distAlong = RandRange(rng, 0.0f, windRange_);
  const float distSide = RandRange(rng, -windRadius_, windRadius_);
  const float distZ = RandRange(rng, -0.03f, 0.03f);

  Vector3 pos = Add(emitterPos, Multiply(windDir_, distAlong));
  pos = Add(pos, Multiply(windRight_, distSide));
  pos = Add(pos, {0.0f, 0.0f, distZ});

  const float w = RandRange(rng, widthMin_, widthMax_);
  const float l = RandRange(rng, lengthMin_, lengthMax_);

  p.transform.scale = {w, l, 1.0f};
  p.transform.rotation = {0.0f, 0.0f, std::atan2(windDir_.y, windDir_.x)};
  p.transform.translation = pos;

  const float speedJitter = RandRange(rng, 0.8f, 1.2f);
  p.velocity = Multiply(windDir_, windSpeed_ * speedJitter);

  p.color = {0.6f, 0.9f, 1.0f, 1.0f};

  const float life = windRange_ / (std::max)(windSpeed_, 0.01f);
  p.lifeTime = life * RandRange(rng, 0.85f, 1.15f);
  p.currentTime = 0.0f;
}

void WindParticle::UpdateOneParticle(ParticleData &p, float dt) {
  p.transform.translation = Add(p.transform.translation, p.velocity);
  p.currentTime += dt;
}

float WindParticle::ComputeAlpha(const ParticleData &p) const {
  if (p.lifeTime <= 0.0f) {
    return 0.0f;
  }

  float t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);
  float fadeIn = SmoothStep(0.0f, 0.2f, t);
  float fadeOut = SmoothStep(0.0f, 0.2f, 1.0f - t);
  return fadeIn * fadeOut;
}

} // namespace RC
