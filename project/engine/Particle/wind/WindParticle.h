#pragma once
#include "Particle.h"

namespace RC {

class WindParticle : public Particle {
public:
  void Initialize(SceneContext &ctx);

  void SetWind(const Vector3 &origin, const Vector3 &force, float range,
               float radius, bool active);

  void SetActive(bool active);

protected:
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;
  void UpdateOneParticle(ParticleData &p, float dt) override;
  float ComputeAlpha(const ParticleData &p) const override;

private:
  Vector3 windDir_{1.0f, 0.0f, 0.0f};
  Vector3 windRight_{0.0f, 1.0f, 0.0f};
  float windSpeed_ = 0.25f;
  float windRange_ = 6.0f;
  float windRadius_ = 0.4f;

  float widthMin_ = 0.08f;
  float widthMax_ = 0.15f;
  float lengthMin_ = 0.8f;
  float lengthMax_ = 1.6f;

  float speedScale_ = 3.0f;
};

} // namespace RC
