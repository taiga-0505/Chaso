#pragma once
#include "Particle.h"

namespace RC {

class ExplosionParticle : public Particle {
public:
  // 爆発を発生させるトリガー（中心位置を渡す）
  void Trigger(const Vector3 &position);

protected:
  // 爆発用のテクスチャ（用意できるまでは circle でもOK）
  const char *GetTexturePath() const override;

  // パーティクル1個分の初期化（速度・色・寿命など）
  void InitParticleCore(ParticleData &particle, std::mt19937 &randomEngine,
                        const Vector3 &emitterTranslate) override;

  // パーティクル1個分の更新
  void UpdateOneParticle(ParticleData &particle, float deltaTime) override;

private:
  // 爆発用のパラメータ（あとで ImGui からいじりたくなったら public
  // に出してもOK）
  float minSpeed_ = 0.3f; // 最低速度
  float maxSpeed_ = 1.5f; // 最大速度
  float minLife_ = 0.3f;  // 最短寿命
  float maxLife_ = 1.0f;  // 最長寿命
};

} // namespace RC
