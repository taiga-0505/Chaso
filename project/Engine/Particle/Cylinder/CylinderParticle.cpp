#include "CylinderParticle.h"
#include "Graphics/Mesh/MeshGenerator.h"
#include "Common/Math/Math.h"

namespace RC {

void CylinderParticle::Initialize(SceneContext &ctx) {
  Particle::Initialize(ctx);

  // AutoSpawnを無効化（スクリプト・演出側から手動で放出するため）
  SetEmitterAutoSpawn(false);
  GetEmitter().count = 1;

  // ビルボードを無効化（シリンダーそのものの形を見せるため）
  SetUseBillboard(false);
  SetUseAccelerationField(false);
  SetEmitterAutoSpawn(true); // テスト用に自動スポーンを有効化

  // メッシュの生成
  float topRadius = 1.0f;
  float bottomRadius = 1.0f;
  float height = 1.5f;
  uint32_t segments = 32;

  ModelData md = MeshGenerator::GenerateEffectCylinder(
      topRadius, bottomRadius, height, segments, 0.0f, 360.0f, false, true);

  md.material.textureFilePath = GetTexturePath();
  InitializeWithModel(ctx, md);

  GetEmitter().count = 1;
  GetEmitter().frequency = 1.0f; // 1.0秒に1回スポーン

  // CommonInit_ で生成された初期パーティクルをクリア
  particles.clear();
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
  p.transform.rotation.y += dt * 2.0f;
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
