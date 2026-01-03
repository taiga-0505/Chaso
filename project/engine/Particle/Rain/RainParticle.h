#pragma once
#include "Particle.h"

namespace RC {

// 雨用パーティクル
//  - エミッタの上空の広い範囲から落ちてくる
//  - 細長いしずくが、重力でだんだん加速しながら落ちるイメージ
class RainParticle : public Particle {
public:
  // 初期化
  void Initialize(SceneContext &ctx);

protected:
  // 雨用テクスチャ
  const char *GetTexturePath() const override {
    // 好きなテクスチャに変えてOK
    return "Resources/Particle/circle.png";
  }

  // 1個分の初期化（出現位置・大きさ・速度・寿命など）
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  // 1個分の更新（重力で加速させたり、回転を止めたり）
  void UpdateOneParticle(ParticleData &p, float dt) override;
};

} // namespace RC
