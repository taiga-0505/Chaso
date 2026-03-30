#pragma once
#include "Particle.h"
#include <random>

namespace RC {

// ==================
// レーザー着弾スパーク
//  - 着弾点からコーン状に飛び散る
//  - 板ポリを「速度方向」に伸ばして火花っぽくする
// ==================

struct ImpactEmitDesc {
  float interval = 0.03f; // 何秒ごとに出すか（0以下なら毎フレ）
  int countPerTick = 6;   // 1回で出す粒数
  int burstOnStart = 24;  // 開始時にドカッと（0なら無し）
};

class ImpactSparkParticle : public Particle {
public:
  void Initialize(SceneContext &ctx);

  // その場で count 個スポーン
  void Burst(const Vector3 &pos, const Vector3 &normal, int count = 10);

  // Laserみたいに「startend渡すだけ」で着弾スパークを出す
  void SetImpactFromBeam(const Vector3 &start, const Vector3 &end,
                         const ImpactEmitDesc &desc = ImpactEmitDesc{});

  // すでに hitNormal が取れてるならこっち（計算すら不要）
  void SetImpact(const Vector3 &hitPos, const Vector3 &hitNormal,
                 const ImpactEmitDesc &desc = ImpactEmitDesc{});

  // レーザー止まった時に呼ぶ（開始バースト判定やタイマーをリセット）
  void StopImpact();

  // 幅付き（レーザーの幅からスポーン位置を散らす）
  void SetImpactFromBeam(const Vector3 &start, const Vector3 &end,
                         float beamWidth,
                         const ImpactEmitDesc &desc = ImpactEmitDesc{});

  // 調整用：幅に対してどれくらい散らすか（0.35〜0.5おすすめ）
  void SetImpactRadiusFactor(float f) { impactRadiusFactor_ = f; }

  // めり込み/チラつき防止で法線方向に少し浮かせる
  void SetSurfaceBias(float b) { surfaceBias_ = b; }

  // 調整用（好きにいじってOK）
  void SetConeAngleDeg(float deg) { coneAngleDeg_ = deg; }
  void SetSpeedRange(float minV, float maxV) {
    speedMin_ = minV;
    speedMax_ = maxV;
  }
  void SetLifeRange(float minL, float maxL) {
    lifeMin_ = minL;
    lifeMax_ = maxL;
  }
  void SetStreakRange(float minLen, float maxLen) {
    streakMin_ = minLen;
    streakMax_ = maxLen;
  }
  void SetThickness(float t) { thickness_ = t; }
  void SetGravity(float g) { gravity_ = g; }
  void SetDamping(float d) { damping_ = d; }
  void SetColors(Vector4 start, Vector4 end) {
    startColor_ = start;
    endColor_ = end;
  }

protected:
  const char *GetTexturePath() const override {
    // もっと火花っぽいテクスチャがあるなら差し替えるとさらに良い
    return "Resources/Particle/circle.png";
  }

  // 1粒の初期化
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  // 1フレームの更新
  void UpdateOneParticle(ParticleData &p, float dt) override;

  // パーティクルのワールド行列構築
  Matrix4x4 BuildWorldMatrix(const ParticleData &p,
                             const Matrix4x4 &billboardMatrix) const override;

  // フェード用アルファ計算
  float ComputeAlpha(const ParticleData &p) const override;

private:
  // 最大数を超えた粒子の破棄
  void TrimToMax_();

  // ===== 放出制御 =====
  ImpactEmitDesc emitDesc_{};
  float emitTimer_ = 0.0f;
  bool emitting_ = false;

  // Burst のときに一時的に使う「今回の法線」
  Vector3 burstNormal_ = {0.0f, 1.0f, 0.0f};

  // ===== 見た目調整パラメータ =====
  float coneAngleDeg_ = 70.0f; // 法線中心にこの角度の円錐内に飛ぶ
  float speedMin_ = 0.03f;     // 1フレーム当たり移動量（今のParticle基準）
  float speedMax_ = 0.10f;

  float lifeMin_ = 0.15f; // 秒
  float lifeMax_ = 0.35f;

  float streakMin_ = 0.10f; // 火花の長さ（ワールド）
  float streakMax_ = 0.35f;

  float thickness_ = 0.05f; // 火花の太さ
  float gravity_ = 0.45f;   // 下向き加速度（大きいほど落ちる）
  float damping_ = 0.985f;  // 速度減衰（1に近いほど長く飛ぶ）

  // ===== レーザー幅に応じた散らし =====
  Vector3 impactAxis_ = {0.0f, 1.0f, 0.0f}; // 円盤の法線（基本はレーザー方向）
  float impactRadius_ = 0.0f;               // 円盤半径（ワールド）
  float impactRadiusFactor_ = 0.45f;        // beamWidth * 0.45 くらいが見た目良い
  float surfaceBias_ = 0.01f;               // 法線方向にちょい押し出す

  // ===== 色 =====
  Vector4 startColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 白っぽい火花
  Vector4 endColor_ = {1.0f, 0.6f, 0.1f, 1.0f};   // オレンジ寄り

  int maxParticles_ = 100;

  // Burst用RNG（baseのrandomEngineはprivateなので自前）
  std::random_device rd_;
  std::mt19937 rng_{rd_()};
};

} // namespace RC
