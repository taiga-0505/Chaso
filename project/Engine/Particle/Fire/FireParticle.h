#pragma once
#include "Particle.h"

namespace RC {

/// @class FireParticle
/// @brief 上昇しながら色とサイズが変化する炎パーティクルを制御するクラス
/// @details エミッタの周囲に散らばりながら放出され、上方向へふわっと移動します。
/// 時間経過とともにオレンジから暗い赤へと色が変化し、サイズが縮小しながら消滅します。
class FireParticle : public Particle {
public:
  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

  /// @brief エミッタの周期に合わせてパーティクルを放出する
  /// @param emitter エミッタの設定
  /// @param randomEngine 乱数生成エンジン
  /// @return 生成されたパーティクルデータのリスト
  std::vector<ParticleData> Emit(const Emitter &emitter,
                                 std::mt19937 &randomEngine) override;

protected:
  /// @brief 使用するテクスチャパスを取得する
  /// @return テクスチャパス
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  /// @brief 個々のパーティクルの初期化（炎特有の初速度、寿命、色設定）
  /// @param p パーティクルデータ
  /// @param rng 乱数生成器
  /// @param emitterPos エミッタの座標
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  /// @brief 個々のパーティクルの更新（上昇、色変化、縮小処理）
  /// @param p パーティクルデータ
  /// @param dt 経過時間
  void UpdateOneParticle(ParticleData &p, float dt) override;
};
} // namespace RC
