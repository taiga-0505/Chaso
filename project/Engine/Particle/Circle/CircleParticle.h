#pragma once
#include "Particle.h"

namespace RC {

// 霊魂っぽい炎パーティクル
//  - その場にふわふわ留まりつつ、少しだけ上下＆横にゆれる
//  - 火力に応じて数・明るさ・色が変わる（A/D で操作）
class CircleParticle : public Particle {
public:
  void Initialize(SceneContext &ctx);

  // ==================
  // A / D で火力をいじりながら Update したいとき用
  //  例: fire_.UpdateWithInput(sceneCtx_, view, proj);
  // ==================
  void UpdateWithInput(SceneContext &ctx, const RC::Matrix4x4 &view,
                       const RC::Matrix4x4 &proj);

  // エミッタの周期が来たときの挙動を炎用に差し替え
  std::list<ParticleData> Emit(const Emitter &emitter,
                               std::mt19937 &randomEngine) override;

  // 火力操作API（シーン側からもいじれるように）
  void AddCirclePower(float delta);
  void SetCirclePower(float v);
  float GetCirclePower() const { return firePower_; }

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

private:
  // 0.0 = 消えた状態, 1.0 = 通常の霊魂, 2.0 =かなり強く光る
  float firePower_ = 1.0f;
  float firePowerMin_ = 0.0f;
  float firePowerMax_ = 2.0f;
};

} // namespace RC
