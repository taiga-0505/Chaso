#pragma once
#include "Particle.h"

namespace RC {

/// @class SnowParticle
/// @brief ゆっくりと舞い落ちる雪のパーティクルを制御するクラス
/// @details エミッタの上空の広い範囲から生成され、低い終端速度でふわふわと落下する挙動を実装しています。
/// 季節の演出や冷涼な環境表現に適しています。
class SnowParticle : public Particle {
public:
  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

protected:
  /// @brief 使用するテクスチャパスを取得する
  /// @return テクスチャパス
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  /// @brief 個々のパーティクルの初期化（出現位置、初速度、寿命設定）
  /// @param p パーティクルデータ
  /// @param rng 乱数生成器
  /// @param emitterPos エミッタの中心座標
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  /// @brief 個々のパーティクルの更新（重力の影響を抑えた緩やかな落下処理）
  /// @param p パーティクルデータ
  /// @param dt 経過時間
  void UpdateOneParticle(ParticleData &p, float dt) override;
};

} // namespace RC
