#pragma once
#include "Particle.h"

namespace RC {

// 雪用パーティクル
//  - エミッタの少し上の広い範囲から、ゆっくりふわっと落ちる
class SnowParticle : public Particle {
public:
  // 初期化
  void Initialize(SceneContext &ctx);

protected:
  // 雪用テクスチャ
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  // 1個分の初期化（出現位置・大きさ・速度・寿命など）
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  // 1個分の更新（重力やふわふわ感の調整）
  void UpdateOneParticle(ParticleData &p, float dt) override;
};

} // namespace RC
