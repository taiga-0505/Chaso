#include "CylinderParticle.h"
#include "Graphics/Mesh/MeshGenerator.h"
#include "Common/Math/Math.h"

namespace RC {

void CylinderParticle::Initialize(SceneContext &ctx) {
  CylinderParam defaultParam;
  Initialize(ctx, defaultParam);
}

void CylinderParticle::Initialize(SceneContext &ctx, const CylinderParam &param) {
  Particle::Initialize(ctx);

  // AutoSpawnを無効化（スクリプト・演出側から手動で放出するため）
  SetEmitterAutoSpawn(false);
  GetEmitter().count = 1;

  // ビルボードを無効化（シリンダーそのものの形を見せるため）
  SetUseBillboard(false);
  SetUseAccelerationField(false);

  // メッシュの生成
  ModelData md = MeshGenerator::GenerateEffectCylinder(
      param.topRadius, param.bottomRadius, param.height, param.segments,
      param.startAngle, param.endAngle, param.isVerticalUV, param.flipV);

  md.material.textureFilePath = GetTexturePath();
  InitializeWithModel(ctx, md);

  GetEmitter().count = 1;
  GetEmitter().frequency = 1.0f; // 1.0秒に1回スポーン

  // CommonInit_ で生成された初期パーティクルをクリア
  particles.clear();
}

void CylinderParticle::Trigger(const Vector3 &pos) {
  SetEmitterTranslation(pos);
  AddParticlesFromEmitter();
}

void CylinderParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                        const Vector3 &emitterPos) {
  // 初期位置を少し下にずらす
  p.transform.translation = emitterPos;
  p.transform.translation.y -= 1.5f;
  // 回転
  p.transform.rotation = {0.0f, 0.0f, 0.0f};
  // スケール
  p.transform.scale = {1.0f, 1.0f, 1.0f};

  // 速度（上方向へ少し移動するなど）
  p.velocity = {0.0f, 2.0f, 0.0f};

  // 寿命（チカチカを抑えるために長めに設定）
  std::uniform_real_distribution<float> distLife(2.0f, 3.0f);
  p.lifeTime = distLife(rng);
  p.currentTime = 0.0f;

  // 基本カラー
  p.color = {1.0f, 1.0f, 1.0f, 1.0f};
}

void CylinderParticle::UpdateOneParticle(ParticleData &p, float dt) {
  p.currentTime += dt;

  // Y軸回転
  p.transform.rotation.y += dt * rotationSpeedY_;
}

float CylinderParticle::ComputeAlpha(const ParticleData &p) const {
  float ratio = p.currentTime / p.lifeTime;
  // Fade in -> Fade out
  if (ratio < 0.2f) {
      return ratio / 0.2f;
  }
  return 1.0f - ((ratio - 0.2f) / 0.8f);
}

} // namespace RC
