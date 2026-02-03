#pragma once
#include "Particle.h"

namespace RC {

class LaserParticle : public Particle {
public:
  void Initialize(SceneContext &ctx);

  // start〜end をビームとして描画
  void SetBeam(const Vector3 &start, const Vector3 &end, float width = 0.2f,
               float life = 0.05f, Vector4 color = {0.2f, 0.9f, 1.0f, 1.0f});

  // 流れの調整（お好み）
  void SetFlowParams(float segmentSpacing, float scrollSpeed,
                     int maxSegments = 64) {
    segmentSpacing_ = segmentSpacing;
    scrollSpeed_ = scrollSpeed;
    maxSegments_ = maxSegments;
  }

protected:
  const char *GetTexturePath() const override {
    // circleでもOKだけど、細長い発光テクスチャにするともっとビームっぽくなる
    return "Resources/Particle/circle.png";
  }

  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  void UpdateOneParticle(ParticleData &p, float dt) override;

  Matrix4x4 BuildWorldMatrix(const ParticleData &p,
                             const Matrix4x4 &billboardMatrix) const override;

  float ComputeAlpha(const ParticleData &p) const override;

private:
  float width_ = 0.2f;
  float life_ = 0.05f;
  Vector4 color_ = {0.2f, 0.9f, 1.0f, 1.0f};

  // ===== 追加：ビーム情報 =====
  Vector3 beamStart_ = {0, 0, 0};
  Vector3 beamDir_ = {0, 1, 0}; // 正規化済み
  float beamLen_ = 0.0f;

  // ===== 追加：流れ設定 =====
  float segmentSpacing_ = 0.15f; // 粒の間隔（小さいほど滑らか）
  float scrollSpeed_ = 6.0f;     // 流れる速度（ワールド単位/秒）
  int maxSegments_ = 64;
};

} // namespace RC
