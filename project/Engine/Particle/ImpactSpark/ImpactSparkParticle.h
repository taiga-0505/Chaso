#pragma once
#include "Particle.h"
#include <random>

namespace RC {

// ==================
// レーザー着弾スパーク
//  - 着弾点からコーン状に飛び散る
//  - 板ポリを「速度方向」に伸ばして火花っぽくする
// ==================

/// @struct ImpactEmitDesc
/// @brief 着弾スパークの放出設定を保持する構造体
struct ImpactEmitDesc {
  float interval = 0.03f; ///< 放出間隔（秒）。0以下の場合は毎フレーム放出。
  int countPerTick = 6;   ///< 1回の放出タイミングで生成するパーティクル数
  int burstOnStart = 24;  ///< 放出開始時に一斉に生成するパーティクル数
};

/// @class ImpactSparkParticle
/// @brief レーザーの着弾や衝突時に発生する火花を制御するクラス
/// @details 板ポリを移動方向に引き延ばす「ストレッチ」表現を行い、火花らしい見た目を実現します。
/// 法線に基づいた円錐状（コーン状）の飛散、重力による落下、速度減衰などの物理挙動を設定可能です。
class ImpactSparkParticle : public Particle {
public:
  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

  /// @brief 指定した位置に一斉にパーティクルを生成する
  /// @param pos 生成位置
  /// @param normal 飛散方向の基準となる法線
  /// @param count 生成する数
  void Burst(const Vector3 &pos, const Vector3 &normal, int count = 10);

  /// @brief ビームの始点と終点から、自動的に終点（着弾点）へスパークを設定する
  /// @param start ビーム始点
  /// @param end ビーム終点（着弾点）
  /// @param desc 放出設定
  void SetImpactFromBeam(const Vector3 &start, const Vector3 &end,
                         const ImpactEmitDesc &desc = ImpactEmitDesc{});

  /// @brief 着弾点と法線が既知の場合に、直接スパークを設定する
  /// @param hitPos 着弾座標
  /// @param hitNormal 着弾面の法線
  /// @param desc 放出設定
  void SetImpact(const Vector3 &hitPos, const Vector3 &hitNormal,
                 const ImpactEmitDesc &desc = ImpactEmitDesc{});

  /// @brief スパークの放出を停止し、内部タイマーをリセットする
  void StopImpact();

  /// @brief ビームの幅を考慮して、着弾点周囲に散らしながらスパークを設定する
  /// @param start ビーム始点
  /// @param end ビーム終点
  /// @param beamWidth ビームの半径
  /// @param desc 放出設定
  void SetImpactFromBeam(const Vector3 &start, const Vector3 &end,
                         float beamWidth,
                         const ImpactEmitDesc &desc = ImpactEmitDesc{});

  /// @brief ビーム幅に対してどの程度散らすかの係数を設定する
  /// @param f 散らし係数（推奨: 0.35 ～ 0.5）
  void SetImpactRadiusFactor(float f) { impactRadiusFactor_ = f; }

  /// @brief 地面へのめり込みを防ぐためのオフセット（法線方向への浮かせ）を設定する
  /// @param b バイアス値
  void SetSurfaceBias(float b) { surfaceBias_ = b; }

  /// @brief 飛散する円錐の広がり角度を設定する
  /// @param deg 角度（度数法）
  void SetConeAngleDeg(float deg) { coneAngleDeg_ = deg; }

  /// @brief パーティクルの初速度範囲を設定する
  /// @param minV 最小速度
  /// @param maxV 最大速度
  void SetSpeedRange(float minV, float maxV) {
    speedMin_ = minV;
    speedMax_ = maxV;
  }

  /// @brief パーティクルの寿命範囲を設定する
  /// @param minL 最小寿命（秒）
  /// @param maxL 最大寿命（秒）
  void SetLifeRange(float minL, float maxL) {
    lifeMin_ = minL;
    lifeMax_ = maxL;
  }

  /// @brief 火花の長さの範囲を設定する
  /// @param minLen 最小長
  /// @param maxLen 最大長
  void SetStreakRange(float minLen, float maxLen) {
    streakMin_ = minLen;
    streakMax_ = maxLen;
  }

  /// @brief 火花の太さを設定する
  /// @param t 太さ
  void SetThickness(float t) { thickness_ = t; }

  /// @brief 下向きの重力加速度を設定する
  /// @param g 重力値（大きいほど早く落ちる）
  void SetGravity(float g) { gravity_ = g; }

  /// @brief 速度の減衰率を設定する
  /// @param d 減衰率（1.0 に近いほど長く飛ぶ）
  void SetDamping(float d) { damping_ = d; }

  /// @brief パーティクルの開始色と終了色を設定する
  /// @param start 開始カラー
  /// @param end 終了カラー（消滅直前の色）
  void SetColors(Vector4 start, Vector4 end) {
    startColor_ = start;
    endColor_ = end;
  }

protected:
  /// @brief 使用するテクスチャパスを取得する
  /// @return テクスチャパス
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  /// @brief 個々のパーティクルの初期化
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  /// @brief 個々のパーティクルの更新
  void UpdateOneParticle(ParticleData &p, float dt) override;

  /// @brief 火花を移動方向に伸ばすための行列構築
  void BuildWorldMatrix(const ParticleData &p,
                             const Matrix4x4 &billboardMatrix) const override;

  /// @brief フェード用アルファ計算
  float ComputeAlpha(const ParticleData &p) const override;

private:
  /// @brief 最大数を超えたパーティクルを古い順に破棄する
  void TrimToMax_();

private:
  // ===== 放出制御 =====
  ImpactEmitDesc emitDesc_{}; ///< 放出設定
  float emitTimer_ = 0.0f;   ///< 放出タイマー
  bool emitting_ = false;    ///< 放出中フラグ

  Vector3 burstNormal_ = {0.0f, 1.0f, 0.0f}; ///< バースト放出時の法線基準

  // ===== 見た目調整パラメータ =====
  float coneAngleDeg_ = 70.0f; ///< 飛散角度
  float speedMin_ = 0.03f;     ///< 最小速度
  float speedMax_ = 0.10f;     ///< 最大速度

  float lifeMin_ = 0.15f; ///< 最小寿命
  float lifeMax_ = 0.35f; ///< 最大寿命

  float streakMin_ = 0.10f; ///< 火花の最小長
  float streakMax_ = 0.35f; ///< 火花の最大長

  float thickness_ = 0.05f; ///< 火花の太さ
  float gravity_ = 0.45f;   ///< 重力の影響度
  float damping_ = 0.985f;  ///< 速度減衰率

  // ===== レーザー幅に応じた散らし =====
  Vector3 impactAxis_ = {0.0f, 1.0f, 0.0f}; ///< 着弾面の法線
  float impactRadius_ = 0.0f;               ///< 散布半径
  float impactRadiusFactor_ = 0.45f;        ///< 半径係数
  float surfaceBias_ = 0.01f;               ///< 法線バイアス

  // ===== 色 =====
  Vector4 startColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; ///< 開始色
  Vector4 endColor_ = {1.0f, 0.6f, 0.1f, 1.0f};   ///< 終了色

  int maxParticles_ = 100; ///< 最大パーティクル保持数

  std::random_device rd_; ///< 乱数デバイス
  std::mt19937 rng_{rd_()}; ///< 乱数生成エンジン
};

} // namespace RC
