#pragma once
#include "Particle.h"
#include <random>

namespace RC {

class LaserParticle : public Particle {
public:
  enum class Style { Segment, Streak };

  void Initialize(SceneContext &ctx);

  void SetBeam(const Vector3 &start, const Vector3 &end, float width = 0.2f,
               float life = 0.05f, Vector4 color = {0.2f, 0.9f, 1.0f, 1.0f});

  void SetFlowParams(float segmentSpacing, float scrollSpeed,
                     int maxSegments = 64) {
    segmentSpacing_ = segmentSpacing;
    scrollSpeed_ = scrollSpeed;
    maxSegments_ = maxSegments;
  }

  // ★追加：見た目モード
  void SetStyle(Style s) { style_ = s; }

  // 縦筋（ストリーク）設定

  /// <summary>
  /// ストリークモードのパラメータ設定
  /// </summary>
  /// <param name="count">ストリーク本数</param>
  /// <param name="minLen">ストリーク最小長さ</param>
  /// <param name="maxLen">ストリーク最大長さ</param>
  /// <param name="streakWidth">ストリーク横幅</param>
  /// <paramname="spreadZRatio">断面の奥行き散らし比率（縦筋なら小さめがオススメ）</param>
  void SetStreakParams(int count, float minLen, float maxLen, float streakWidth,
                       float spreadZRatio = 0.15f) {
    streakCount_ = count;
    streakMinLen_ = minLen;
    streakMaxLen_ = maxLen;
    streakWidth_ = streakWidth;
    streakSpreadZRatio_ = spreadZRatio;
  }

protected:
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;
  void UpdateOneParticle(ParticleData &p, float dt) override;

  Matrix4x4 BuildWorldMatrix(const ParticleData &p,
                             const Matrix4x4 &billboardMatrix) const override;

  float ComputeAlpha(const ParticleData &p) const override;

private:
  void RespawnStreak(ParticleData &p);

  float width_ = 0.2f;
  float life_ = 0.05f;
  Vector4 color_ = {0.2f, 0.9f, 1.0f, 1.0f};

  Vector3 beamStart_ = {0, 0, 0};
  Vector3 beamDir_ = {0, 1, 0};
  float beamLen_ = 0.0f;

  // ★追加：ビーム断面の基底（オフセット用）
  Vector3 beamRight_ = {1, 0, 0};
  Vector3 beamForward_ = {0, 0, 1};

  float segmentSpacing_ = 0.15f;
  float scrollSpeed_ = 6.0f;
  int maxSegments_ = 64;

  // ストリーク設定
  Style style_ = Style::Segment;
  int streakCount_ = 120;
  float streakMinLen_ = 0.15f;
  float streakMaxLen_ = 0.8f;
  float streakWidth_ = 0.03f;
  float streakSpreadZRatio_ = 0.15f;

  std::mt19937 rng_{1234567};
};

} // namespace RC
