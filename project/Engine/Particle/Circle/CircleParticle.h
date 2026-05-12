#pragma once
#include "Particle.h"

namespace RC {

/// @class CircleParticle
/// @brief 霊魂や炎のような円形パーティクルを制御するクラス
/// @details パーティクルがその場に留まりつつ、上下左右に揺れる挙動を実装しています。
/// 「火力（firePower）」の概念を持ち、入力や API 経由で数・明るさ・色を動的に変更可能です。
class CircleParticle : public Particle {
public:
  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

  /// @brief 入力（A/Dキー）を受け取りつつパーティクルを更新する
  /// @param ctx シーンコンテキスト
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  void UpdateWithInput(SceneContext &ctx, const RC::Matrix4x4 &view,
                       const RC::Matrix4x4 &proj);

  /// @brief エミッタからパーティクルを放出する
  /// @param emitter エミッタの設定
  /// @param randomEngine 乱数生成エンジン
  /// @return 生成されたパーティクルデータのリスト
  std::vector<ParticleData> Emit(const Emitter &emitter,
                                 std::mt19937 &randomEngine) override;

  /// @brief 火力値を増減させる
  /// @param delta 変化量
  void AddCirclePower(float delta);

  /// @brief 火力値を直接設定する
  /// @param v 設定する値（通常 0.0 ～ 2.0）
  void SetCirclePower(float v);

  /// @brief 現在の火力値を取得する
  /// @return 火力値
  float GetCirclePower() const { return firePower_; }

protected:
  /// @brief 使用するテクスチャパスを取得する
  /// @return テクスチャパス
  const char *GetTexturePath() const override {
    return "Resources/Particle/circle.png";
  }

  /// @brief 個々のパーティクルの初期化（炎特有のパラメータ設定）
  /// @param p パーティクルデータ
  /// @param rng 乱数生成器
  /// @param emitterPos エミッタの座標
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  /// @brief 個々のパーティクルの更新（揺らぎや火力に応じた変化）
  /// @param p パーティクルデータ
  /// @param dt 経過時間
  void UpdateOneParticle(ParticleData &p, float dt) override;

private:
  float firePower_ = 1.0f;    ///< 現在の火力 (0.0:消滅, 1.0:通常, 2.0:強光)
  float firePowerMin_ = 0.0f; ///< 火力の最小値
  float firePowerMax_ = 2.0f; ///< 火力の最大値
};

} // namespace RC