#include "RingParticle.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace RC {

void RingParticle::Initialize(SceneContext &ctx) {
  // =============================================
  // 資料準拠: XY平面上のリングメッシュを直接生成
  // インデックスバッファ不使用（Particle基盤が DrawInstanced のため）
  // 四角形1つにつき頂点6個（三角形2つ）
  // =============================================
  const uint32_t kRingDivide = 32;
  const float kOuterRadius = 1.0f;
  const float kInnerRadius = 0.2f;
  const float radianPerDivide =
      2.0f * std::numbers::pi_v<float> / float(kRingDivide);

  ModelData ring;

  for (uint32_t index = 0; index < kRingDivide; ++index) {
    float s = std::sin(index * radianPerDivide);
    float c = std::cos(index * radianPerDivide);
    float sNext = std::sin((index + 1) * radianPerDivide);
    float cNext = std::cos((index + 1) * radianPerDivide);
    float u = float(index) / float(kRingDivide);
    float uNext = float(index + 1) / float(kRingDivide);

    // 資料通りの4頂点:
    // ① 外側 current
    VertexData v1;
    v1.position = {-s * kOuterRadius, c * kOuterRadius, 0.0f, 1.0f};
    v1.texcoord = {u, 0.0f};
    v1.normal = {0.0f, 0.0f, 1.0f};

    // ② 外側 next
    VertexData v2;
    v2.position = {-sNext * kOuterRadius, cNext * kOuterRadius, 0.0f, 1.0f};
    v2.texcoord = {uNext, 0.0f};
    v2.normal = {0.0f, 0.0f, 1.0f};

    // ③ 内側 current
    VertexData v3;
    v3.position = {-s * kInnerRadius, c * kInnerRadius, 0.0f, 1.0f};
    v3.texcoord = {u, 1.0f};
    v3.normal = {0.0f, 0.0f, 1.0f};

    // ④ 内側 next
    VertexData v4;
    v4.position = {-sNext * kInnerRadius, cNext * kInnerRadius, 0.0f, 1.0f};
    v4.texcoord = {uNext, 1.0f};
    v4.normal = {0.0f, 0.0f, 1.0f};

    // 三角形1: ①③② (カリング対策で時計回りに)
    ring.vertices.push_back(v1);
    ring.vertices.push_back(v3);
    ring.vertices.push_back(v2);

    // 三角形2: ②③④ (カリング対策で時計回りに)
    ring.vertices.push_back(v2);
    ring.vertices.push_back(v3);
    ring.vertices.push_back(v4);
  }

  // 基盤クラスをこのメッシュで初期化
  InitializeWithModel(ctx, ring);

  // ビルボード無効（3Dメッシュとしてそのまま表示）
  SetUseBillboard(false);

  // 自動スポーン無効（トリガー時のみ放出）
  SetEmitterAutoSpawn(false);
  GetEmitter().count = 1;

  // CommonInit_ で生成された初期パーティクルをクリア
  particles.clear();
}

void RingParticle::InitParticleCore(ParticleData &p, std::mt19937 &rng,
                                      const Vector3 &emitterPos) {
  p.transform.translation = emitterPos;
  p.transform.rotation = {0.0f, 0.0f, 0.0f};
  p.transform.scale = {0.6f, 0.6f, 0.6f}; // サイズを少し小さめに設定

  p.velocity = {0.0f, 0.0f, 0.0f};
  p.color = {1.0f, 1.0f, 1.0f, 1.0f};
  p.lifeTime = 0.8f; // Flashが消え始める時間に合わせる
  p.currentTime = 0.0f;
}

void RingParticle::UpdateOneParticle(ParticleData &p, float dt) {
  p.currentTime += dt;
}

float RingParticle::ComputeAlpha(const ParticleData &p) const {
  if (p.lifeTime <= 0.0f) return 0.0f;
  float t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);
  // 非線形フェードアウト
  return (1.0f - t) * (1.0f - t);
}

} // namespace RC
