#pragma once
#include "Particle.h"

namespace RC {

/// @class ExplosionParticle
/// @brief 爆発エフェクトを制御するパーティクルクラス
/// @details 指定した中心点から全方位に飛散する爆発演出を実装します。
/// ランダムな初速度と寿命によって、拡散しながら消滅する挙動を提供します。
class ExplosionParticle : public Particle {
public:
  /// @brief 爆発を発生させる
  /// @param position 爆発の中心位置
  void Trigger(const Vector3 &position);

protected:
  /// @brief 使用するテクスチャパスを取得する
  /// @return テクスチャパス
  const char *GetTexturePath() const override;

  /// @brief 個々のパーティクルの初期化（速度、色、寿命のランダム設定）
  /// @param particle パーティクルデータ
  /// @param randomEngine 乱数生成エンジン
  /// @param emitterTranslate 放出中心座標
  void InitParticleCore(ParticleData &particle, std::mt19937 &randomEngine,
                        const Vector3 &emitterTranslate) override;

  /// @brief 個々のパーティクルの更新（移動と速度減衰）
  /// @param particle パーティクルデータ
  /// @param deltaTime 経過時間
  void UpdateOneParticle(ParticleData &particle, float deltaTime) override;

private:
  float minSpeed_ = 0.3f; ///< パーティクルの最低初速度
  float maxSpeed_ = 1.5f; ///< パーティクルの最高初速度
  float minLife_ = 0.3f;  ///< パーティクルの最短寿命
  float maxLife_ = 1.0f;  ///< パーティクルの最長寿命
};

} // namespace RC
