#pragma once
#include "Particle/Particle.h"

namespace RC {

/// <summary>
/// 資料(Reference)に基づいたリングエフェクト用パーティクル
/// - MeshGenerator::GenerateRingEx で生成したメッシュを使用
/// - 時間経過とともに拡大し、フェードアウトする
/// </summary>
class RingParticle : public Particle {
public:
  void Initialize(SceneContext &ctx);

protected:
  const char *GetTexturePath() const override {
    // 本来は gradationLine.png だが、代用として circle.png 等を使用可能
    return "Resources/Particle/circle.png";
  }

  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  void UpdateOneParticle(ParticleData &p, float dt) override;

private:
  float expansionSpeed_ = 5.0f; // 拡大速度
};

} // namespace RC
