#include "RingParticle.h"
#include "Graphics/Mesh/MeshGenerator.h"

namespace RC {

void RingParticle::Initialize(SceneContext &ctx) {
  // 1. リングメッシュを作成（XY平面、全周）
  ModelData ring = MeshGenerator::GenerateRingEx(
      0.8f,  // 内径
      1.0f,  // 外径
      32,    // 分割数
      {1, 1, 1, 1}, // 内側カラー（シェーダ未対応ならUVで対応可）
      {1, 1, 1, 1}, // 外側カラー
      0.0f, 360.0f, // 角度
      false,        // UV方向
      false         // XZ平面 (地面向きを想定)
  );

  // 2. 基盤クラスをこのメッシュで初期化
  InitializeWithModel(ctx, ring);

  // 一括設定
  SetEmitterAutoSpawn(false);
  GetEmitter().count = 1;
}

void RingParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                      const Vector3 &emitterPos) {
  // 基本パラメータ
  p.transform.translation = emitterPos;
  p.transform.rotation = {0.0f, 0.0f, 0.0f};
  p.transform.scale = {0.1f, 0.1f, 0.1f}; // 最初は小さく

  p.velocity = {0.0f, 0.0f, 0.0f};
  p.color = {1.0f, 1.0f, 1.0f, 1.0f}; // 個別カラー
  p.lifeTime = 0.5f;                  // 短命
  p.currentTime = 0.0f;
}

void RingParticle::UpdateOneParticle(ParticleData &p, float dt) {
  // 時間とともに拡大
  float s = expansionSpeed_ * dt;
  p.transform.scale.x += s;
  p.transform.scale.z += s; // XZ平面なのでX, Zを拡大

  p.currentTime += dt;
}

} // namespace RC
