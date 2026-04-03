#pragma once
#include "Particle.h"

namespace RC {

/// <summary>
/// 資料(Reference)に基づいた閃光エフェクト用パーティクル
/// - circle2.png を使用
/// - 放出時に細長いスケールとランダムなZ回転を設定
/// </summary>
class FlashParticle : public Particle {
public:
  void Initialize(SceneContext &ctx);

  // 一括制御用の便利メソッド
  void SetGlobalColor(const Vector4 &color) { EmitterRef().globalColor = color; }
  void SetGlobalScale(const Vector3 &scale) { EmitterRef().globalScale = scale; }
  void SetTimeScale(float scale) { EmitterRef().timeScale = scale; }

protected:
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle2.png";
  }

  // 1粒の初期化（資料通りのパラメータ設定）
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  // 1フレームの更新
  void UpdateOneParticle(ParticleData &p, float dt) override;

private:
  float speedMin_ = 0.05f;
  float speedMax_ = 0.2f;
  float lifeMin_ = 0.2f;
  float lifeMax_ = 0.5f;

  float scaleYMin_ = 0.4f;
  float scaleYMax_ = 1.5f;
};

} // namespace RC
