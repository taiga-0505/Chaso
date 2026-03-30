#pragma once
#include "Particle.h"

namespace RC {

// 炎用パーティクル
//  - エミッタの周りにちょっと広がって出る
//  - 上方向にふわっと上がりながら小さくなって、色がオレンジ→赤暗く変化
class FireParticle : public Particle {
public:
  void Initialize(SceneContext &ctx);

  // エミッタの周期が来たときの挙動を炎用に差し替え
  std::list<ParticleData> Emit(const Emitter &emitter,
                               std::mt19937 &randomEngine) override;

protected:
  // 炎用テクスチャ
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  // 1個分の初期化
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  // 1個分の更新
  void UpdateOneParticle(ParticleData &p, float dt) override;
};
} // namespace RC
