#pragma once
#include "Particle/Particle.h"

namespace RC {

/// <summary>
/// 資料(Reference)に基づいたリングエフェクト用パーティクル
/// - XY平面上にリングメッシュを直接生成（インデックスなし、6頂点/四角形）
/// - gradationLine.png テクスチャ（UV V方向）を使用
/// - 時間経過とともに拡大し、フェードアウトする
/// - ビルボードなし（3Dメッシュとしてそのまま表示）
/// </summary>
class RingParticle : public Particle {
public:
  void Initialize(SceneContext &ctx) override;

protected:
  const char *GetTexturePath() const override {
    return "Resources/Particle/gradationLine.png";
  }

  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  void UpdateOneParticle(ParticleData &p, float dt) override;

  // 非線形フェードアウト（急速に消える）
  float ComputeAlpha(const ParticleData &p) const override;

private:
  float expansionSpeed_ = 8.0f; // 拡大速度（資料に合わせて調整）
};

} // namespace RC
