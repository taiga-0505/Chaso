#include "LaserParticle.h"
#include <Math/Math.h>
#include <algorithm>
#include <cmath>
#include <random>

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

static inline float RandRange(std::mt19937 &rng, float a, float b) {
  std::uniform_real_distribution<float> dist(a, b);
  return dist(rng);
}
} // namespace

namespace RC {

void LaserParticle::Initialize(SceneContext &ctx) {
  Particle::Initialize(ctx);
  SetEmitterAutoSpawn(false);
  ClearAll();

  std::random_device rd;
  rng_.seed(rd());
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
  beamDir_ = Normalize(dir);

  // ===== ビーム断面の基底（ビームに直交する2軸） =====
  Vector3 any =
      (std::fabs(beamDir_.y) > 0.9f) ? Vector3{1, 0, 0} : Vector3{0, 1, 0};
  beamRight_ = Cross(any, beamDir_);
  if (LenSq(beamRight_) <= 1e-8f)
    beamRight_ = {1, 0, 0};
  beamRight_ = Normalize(beamRight_);
  beamForward_ = Normalize(Cross(beamDir_, beamRight_));

  // =====================================================
  // 1) 従来：等間隔セグメント（今の見た目）
  // =====================================================
  if (style_ == Style::Segment) {
    float spacing = (std::max)(segmentSpacing_, 0.02f);
    int need = (int)std::ceil(beamLen_ / spacing);
    need = std::clamp(need, 6, (std::max)(maxSegments_, 6));

    float segLen = beamLen_ / (float)need;

    if ((int)particles.size() != need) {
      particles.clear();
      particles.resize(need);

      int i = 0;
      for (auto &p : particles) {
        p = ParticleData{};
        p.transform.rotation = {segLen * i, 0, 0}; // s
        p.transform.scale = {width_ * 0.5f, segLen * 0.5f, 1.0f};
        p.velocity = beamDir_;
        p.color = color_;
        p.lifeTime = (std::max)(life_, 1e-3f);
        p.currentTime = 0.0f;

        float s = p.transform.rotation.x;
        p.transform.translation = Add(beamStart_, Multiply(beamDir_, s));
        ++i;
      }
    } else {
      for (auto &p : particles) {
        p.transform.scale = {width_ * 0.5f, segLen * 0.5f, 1.0f};
        p.velocity = beamDir_;
        p.color = color_;
        p.lifeTime = (std::max)(life_, 1e-3f);
        p.currentTime = 0.0f;

        float s = p.transform.rotation.x;
        s = std::fmod(s, beamLen_);
        if (s < 0.0f)
          s += beamLen_;
        p.transform.rotation.x = s;

        p.transform.translation = Add(beamStart_, Multiply(beamDir_, s));
      }
    }
    return;
  }

  // =====================================================
  // 2) 新：縦筋ストリーク（ランダム長さ）
  // =====================================================
  int need = std::clamp(streakCount_, 4, (std::max)(maxSegments_, 4));

  if ((int)particles.size() != need) {
    particles.clear();
    particles.resize(need);
    for (auto &p : particles) {
      p = ParticleData{};
      RespawnStreak(p);
    }
  } else {
    // 毎フレ変わるかもしれない共通パラメータだけ更新（長さ/オフセットは維持）
    float rangeY = width_ * 0.5f;
    float rangeZ = width_ * 0.5f * streakSpreadZRatio_;

    for (auto &p : particles) {
      p.velocity = beamDir_;
      p.color = color_;
      p.transform.scale.x = streakWidth_ * 0.5f; // 細さだけ反映
      p.currentTime = 0.0f;                      // 時間フェードは無効化運用
      p.lifeTime = 1.0f;

      // 幅が変わった時に外へ出すぎた筋をクランプ
      p.transform.rotation.y =
          std::clamp(p.transform.rotation.y, -rangeY, rangeY);
      p.transform.rotation.z =
          std::clamp(p.transform.rotation.z, -rangeZ, rangeZ);

      float headS = p.transform.rotation.x; // ★rotation.x は headS
      float halfLen = p.transform.scale.y;
      float centerS = headS - halfLen; // ★中心に変換

      Vector3 pos = Add(beamStart_, Multiply(beamDir_, centerS));
      pos = Add(pos, Multiply(beamRight_, p.transform.rotation.y));
      pos = Add(pos, Multiply(beamForward_, p.transform.rotation.z));
      p.transform.translation = pos;
    }
  }
}

void LaserParticle::RespawnStreak(ParticleData &p) {
  if (beamLen_ <= 1e-4f)
    return;

  float len = RandRange(rng_, streakMinLen_, streakMaxLen_);
  len = std::clamp(len, 0.02f, beamLen_);
  float halfLen = len * 0.5f;

  float rangeY = width_ * 0.5f;
  float rangeZ = width_ * 0.5f * streakSpreadZRatio_;

  float offY = RandRange(rng_, -rangeY, rangeY);
  float offZ = RandRange(rng_, -rangeZ, rangeZ);

  // ★headSは少し手前（負）から
  float headS = -RandRange(rng_, 0.0f, beamLen_ * 0.10f);

  p.transform.rotation = {headS, offY, offZ}; // x=headS
  p.transform.scale = {streakWidth_ * 0.5f, halfLen, 1.0f};
  p.velocity = beamDir_;
  p.color = color_;
  p.lifeTime = 1.0f;
  p.currentTime = 0.0f;

  float centerS = headS - halfLen;
  Vector3 pos = Add(beamStart_, Multiply(beamDir_, centerS));
  pos = Add(pos, Multiply(beamRight_, offY));
  pos = Add(pos, Multiply(beamForward_, offZ));
  p.transform.translation = pos;
}

void LaserParticle::InitParticleCore(ParticleData &p, std::mt19937 &,
                                     const Vector3 &) {
  p = ParticleData{};
}

void LaserParticle::UpdateOneParticle(ParticleData &p, float dt) {
  p.currentTime += dt;

  if (style_ == Style::Segment) {
    if (beamLen_ > 1e-4f) {
      float s = p.transform.rotation.x + scrollSpeed_ * dt;
      s = std::fmod(s, beamLen_);
      if (s < 0.0f)
        s += beamLen_;
      p.transform.rotation.x = s;
      p.transform.translation = Add(beamStart_, Multiply(beamDir_, s));
    }
    return;
  }

  // ストリーク：中心sを進めて、末尾がendを越えたら再抽選
  // ストリーク：headS を進めて、head が end を越えたら再抽選
  if (beamLen_ <= 1e-4f)
    return;

  float headS = p.transform.rotation.x + scrollSpeed_ * dt;
  float halfLen = p.transform.scale.y;

  if (headS > beamLen_ + 0.01f) {
    RespawnStreak(p);
    return;
  }

  p.transform.rotation.x = headS;

  float centerS = headS - halfLen;
  Vector3 pos = Add(beamStart_, Multiply(beamDir_, centerS));
  pos = Add(pos, Multiply(beamRight_, p.transform.rotation.y));
  pos = Add(pos, Multiply(beamForward_, p.transform.rotation.z));
  p.transform.translation = pos;
}

Matrix4x4
LaserParticle::BuildWorldMatrix(const ParticleData &p,
                                const Matrix4x4 &billboardMatrix) const {
  Vector3 up = Normalize(p.velocity);

  Vector3 viewDir = Normalize({billboardMatrix.m[2][0], billboardMatrix.m[2][1],
                               billboardMatrix.m[2][2]});

  Vector3 right = Cross(viewDir, up);
  if (LenSq(right) <= 1e-8f) {
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
  float base =
      1.0f; // 時間フェードは使わない（SetBeamが毎フレ呼ばれる運用なので）

  if (beamLen_ > 1e-4f) {
    float edge = (std::max)(beamLen_ * 0.10f, 0.05f);

    if (style_ == Style::Segment) {
      float s = p.transform.rotation.x; // center
      float a0 = SmoothStep(0.0f, edge, s);
      float a1 = SmoothStep(0.0f, edge, beamLen_ - s);
      base *= (a0 * a1);
    } else {
      // ストリークは head/tail で端フェード
      float head = p.transform.rotation.x; // ★headS
      float halfLen = p.transform.scale.y;

      // center = head - halfLen なので、tail = center - halfLen = head -
      // 2*halfLen
      float tail = head - 2.0f * halfLen;

      float a0 = SmoothStep(0.0f, edge, head);            // startに入る
      float a1 = SmoothStep(0.0f, edge, beamLen_ - tail); // endから抜ける
      base *= (a0 * a1);
    }
  }

  return base;
}

} // namespace RC
