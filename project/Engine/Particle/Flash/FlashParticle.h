#pragma once
#include "Particle.h"

namespace RC {

/// @class FlashParticle
/// @brief 閃光エフェクト（フラッシュ）用パーティクルクラス
/// @details 放出時に細長いスケール（Y軸方向）とランダムな Z 回転を設定することで、火花や鋭い閃光を表現します。
/// 共通のカラー、スケール、タイムスケールを一括制御するための便利メソッドを提供します。
class FlashParticle : public Particle {
public:
  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

  /// @brief 全てのパーティクルの色を一括設定する
  /// @param color 設定するカラー (RGBA)
  void SetGlobalColor(const Vector4 &color) { EmitterRef().globalColor = color; }

  /// @brief 全てのパーティクルのスケールを一括設定する
  /// @param scale 設定するスケール倍率
  void SetGlobalScale(const Vector3 &scale) { EmitterRef().globalScale = scale; }

  /// @brief エフェクト全体の再生速度を一括設定する
  /// @param scale タイムスケール（1.0 が標準速度）
  void SetTimeScale(float scale) { EmitterRef().timeScale = scale; }

protected:
  /// @brief 使用するテクスチャパスを取得する
  /// @return テクスチャパス
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle2.png";
  }

  /// @brief 個々のパーティクルの初期化（閃光特有の細長い形状とランダム回転）
  /// @param p パーティクルデータ
  /// @param rng 乱数生成器
  /// @param emitterPos エミッタの座標
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  /// @brief 個々のパーティクルの更新（アニメーション進行）
  /// @param p パーティクルデータ
  /// @param dt 経過時間
  void UpdateOneParticle(ParticleData &p, float dt) override;

private:
  float speedMin_ = 0.0f; ///< パーティクルの最低移動速度
  float speedMax_ = 0.0f; ///< パーティクルの最高移動速度
  float lifeMin_ = 0.8f;  ///< パーティクルの最短寿命
  float lifeMax_ = 1.0f;  ///< パーティクルの最長寿命

  float scaleYMin_ = 0.8f; ///< Y軸方向の最小スケール（細長さを決定）
  float scaleYMax_ = 2.5f; ///< Y軸方向の最大スケール（細長さを決定）
};

} // namespace RC
