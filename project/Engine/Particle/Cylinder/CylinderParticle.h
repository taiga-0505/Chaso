#pragma once
#include "Particle/Particle.h"

namespace RC {

/// @class CylinderParticle
/// @brief 円柱形状のメッシュを使用したエフェクト用パーティクルクラス
/// @details MeshGenerator::GenerateEffectCylinder で生成したメッシュを使用し、3D 空間上にそのまま配置されます（ビルボードなし）。
/// グラデーションテクスチャなどを使用して、上昇する光の柱などの演出に適しています。
class CylinderParticle : public Particle {
public:
  struct CylinderParam {
    float topRadius = 1.0f;
    float bottomRadius = 1.0f;
    float height = 1.5f;
    uint32_t segments = 32;
    float startAngle = 0.0f;
    float endAngle = 360.0f;
    bool isVerticalUV = false;
    bool flipV = true;
  };

  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

  /// @brief パラメータ指定付き初期化
  void Initialize(SceneContext &ctx, const CylinderParam &param);

  /// @brief 任意の場所でパーティクルを発生させる
  /// @param pos 発生させるワールド座標
  void Trigger(const Vector3 &pos);

  /// @brief Y軸の回転速度を設定する
  /// @param speed 回転速度
  void SetRotationSpeedY(float speed) { rotationSpeedY_ = speed; }

protected:
  /// @brief 使用するテクスチャパスを取得する
  /// @return テクスチャパス
  const char *GetTexturePath() const override {
    return "Resources/Particle/gradationLine.png";
  }

  /// @brief 個々のパーティクルの初期化（円柱特有のサイズや寿命設定）
  /// @param p パーティクルデータ
  /// @param rng 乱数生成器
  /// @param emitterPos エミッタの座標
  void InitParticleCore(ParticleData &p, std::mt19937 &rng,
                        const Vector3 &emitterPos) override;

  /// @brief 個々のパーティクルの更新（上昇挙動やスケール変化）
  /// @param p パーティクルデータ
  /// @param dt 経過時間
  void UpdateOneParticle(ParticleData &p, float dt) override;

  /// @brief パーティクルのアルファ値を計算する（寿命に応じたフェードアウト）
  /// @param p パーティクルデータ
  /// @return 計算されたアルファ値 (0.0 ～ 1.0)
  float ComputeAlpha(const ParticleData &p) const override;

private:
  float rotationSpeedY_ = 2.0f; ///< Y軸の回転速度
};

} // namespace RC
