#pragma once
#include "Particle.h"

namespace RC {

/// @class WindParticle
/// @brief 風の流れ（空気の筋）を表現するパーティクルクラス
/// @details 特定の方向に向かって細長いパーティクルを流すことで、風の視覚的な表現を行います。
/// 風の基点、方向、範囲、半径などの物理的なパラメータを動的に変更可能です。
class WindParticle : public Particle {
public:
  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

  /// @brief 風のパラメータを一括設定する
  /// @param origin 風の発生源（基点）
  /// @param force 風の方向と強さ
  /// @param range 風が届く距離
  /// @param radius 風の太さ（断面半径）
  /// @param active 放出を有効にするか
  void SetWind(const Vector3 &origin, const Vector3 &force, float range,
               float radius, bool active);

  /// @brief 風の放出状態を切り替える
  /// @param active 有効にするなら true
  void SetActive(bool active);

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

  /// @brief フェード用アルファ計算
  float ComputeAlpha(const ParticleData &p) const override;

private:
  Vector3 windDir_{1.0f, 0.0f, 0.0f};   ///< 風の進行方向（単位ベクトル）
  Vector3 windRight_{0.0f, 1.0f, 0.0f}; ///< 風の断面方向（右方向）
  float windSpeed_ = 0.25f;            ///< 風の移動速度
  float windRange_ = 6.0f;             ///< 風の有効距離
  float windRadius_ = 0.4f;            ///< 風の断面半径

  float widthMin_ = 0.08f;  ///< パーティクルの最小幅
  float widthMax_ = 0.15f;  ///< パーティクルの最大幅
  float lengthMin_ = 0.8f;  ///< パーティクルの最小長
  float lengthMax_ = 1.6f;  ///< パーティクルの最大長

  float speedScale_ = 3.0f; ///< 速度に対するスケール倍率
};

} // namespace RC
