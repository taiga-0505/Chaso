#include "LaserParticle.h"
#include <Math/Math.h>
#include <algorithm>
#include <cmath>

namespace {
static inline float LenSq(const RC::Vector3 &v) {
  return v.x * v.x + v.y * v.y + v.z * v.z;
}
static inline float Len(const RC::Vector3 &v) { return std::sqrt(LenSq(v)); }

static inline float Smooth01(float t) { return t * t * (3.0f - 2.0f * t); }
static inline float SmoothStep(float e0, float e1, float x) {
  if (std::fabs(e1 - e0) < 1e-6f)
    return (x >= e1) ? 1.0f : 0.0f;
  float t = (x - e0) / (e1 - e0);
  t = std::clamp(t, 0.0f, 1.0f);
  return Smooth01(t);
}
} // namespace

namespace RC {

void LaserParticle::Initialize(SceneContext &ctx) {
  Particle::Initialize(ctx);
  SetEmitterAutoSpawn(false);
  ClearAll();
}

void LaserParticle::SetBeam(const Vector3 &start, const Vector3 &end,
                            float width, float life, Vector4 color) {
  width_ = width;
  life_ = life;
  color_ = color;

  Vector3 dir = Subtract(end, start);
  float length = Len(dir);

  if (length <= 1e-4f) {
    beamLen_ = 0.0f;
    ClearAll();
    return;
  }

  beamStart_ = start;
  beamLen_ = length;
  beamDir_ = Normalize(dir); // 正規化

  // セグメント数を決める（間隔ベース）
  float spacing = (std::max)(segmentSpacing_, 0.02f);
  int need = (int)std::ceil(beamLen_ / spacing);
  need = std::clamp(need, 6, (std::max)(maxSegments_, 6));

  float segLen = beamLen_ / (float)need; // ぴったり収まる長さに

  // 初回 or 数が変わった時だけ位相を初期化
  if ((int)particles.size() != need) {
    particles.clear();
    particles.resize(need);

    int i = 0;
    for (auto &p : particles) {
      p = ParticleData{};

      // rotation.x を「ビーム上の距離 s」として使う（0〜beamLen）
      p.transform.rotation = {segLen * i, 0, 0};

      p.transform.scale = {width_ * 0.5f, segLen * 0.5f, 1.0f};
      p.velocity =
          beamDir_; // 向きとして使う（移動速度は scrollSpeed_ で別管理）
      p.color = color_;
      p.lifeTime = (std::max)(life_, 1e-3f);
      p.currentTime = 0.0f;

      float s = p.transform.rotation.x;
      p.transform.translation = Add(beamStart_, Multiply(beamDir_, s));
      ++i;
    }
  } else {
    // 既存の位相(s)は維持して「流れ」を継続
    for (auto &p : particles) {
      p.transform.scale = {width_ * 0.5f, segLen * 0.5f, 1.0f};
      p.velocity = beamDir_;
      p.color = color_;
      p.lifeTime = (std::max)(life_, 1e-3f);
      p.currentTime = 0.0f; // 毎フレ SetBeam される間はフェードしない

      float s = p.transform.rotation.x;
      if (beamLen_ > 1e-4f) {
        s = std::fmod(s, beamLen_);
        if (s < 0.0f)
          s += beamLen_;
      } else {
        s = 0.0f;
      }
      p.transform.rotation.x = s;
      p.transform.translation = Add(beamStart_, Multiply(beamDir_, s));
    }
  }
}

void LaserParticle::InitParticleCore(ParticleData &p, std::mt19937 &,
                                     const Vector3 &) {
  // ここは実質使わない（SetBeamで全部作る）
  p = ParticleData{};
}

void LaserParticle::UpdateOneParticle(ParticleData &p, float dt) {
  // フェード用
  p.currentTime += dt;

  // 流す（位相 s を進めて、translation を更新）
  if (beamLen_ > 1e-4f) {
    float s = p.transform.rotation.x + scrollSpeed_ * dt;
    s = std::fmod(s, beamLen_);
    if (s < 0.0f)
      s += beamLen_;

    p.transform.rotation.x = s;
    p.transform.translation = Add(beamStart_, Multiply(beamDir_, s));
  }
}

Matrix4x4
LaserParticle::BuildWorldMatrix(const ParticleData &p,
                                const Matrix4x4 &billboardMatrix) const {
  // ここは元のままでOK（軸ビルボード）
  Vector3 up = Normalize(p.velocity);

  Vector3 viewDir = Normalize({billboardMatrix.m[2][0], billboardMatrix.m[2][1],
                               billboardMatrix.m[2][2]});

  Vector3 right = Cross(viewDir, up);
  if (LenSq(right) <= 1e-8f) {
    // ほぼ平行なら適当な直交ベクトルで逃がす
    Vector3 any =
        (std::fabs(up.y) > 0.9f) ? Vector3{1, 0, 0} : Vector3{0, 1, 0};
    right = Cross(any, up);
  }
  right = Normalize(right);

  Vector3 forward = Normalize(Cross(up, right));

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

  return Multiply(Multiply(scaleMat, rot), transMat);
}

float LaserParticle::ComputeAlpha(const ParticleData &p) const {
  if (p.lifeTime <= 0.0f)
    return 0.0f;
  float t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);
  float base = 1.0f - t;

  // 端っこを少し薄くして「終点が綺麗」になるように
  if (beamLen_ > 1e-4f) {
    float s = p.transform.rotation.x; // 0..beamLen
    float edge = (std::max)(beamLen_ * 0.12f, segmentSpacing_ * 2.0f); // お好み
    float a0 = SmoothStep(0.0f, edge, s);
    float a1 = SmoothStep(0.0f, edge, beamLen_ - s);
    base *= (a0 * a1);
  }

  return base;
}

} // namespace RC
