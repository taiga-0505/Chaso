#pragma once
#include "Particle.h"
#include <random>

namespace RC {

/// @class LaserParticle
/// @brief レーザービーム本体の視覚表現を制御するパーティクルクラス
/// @details ビームを分割された点（Segment）で表現するスタイルと、引き延ばされた筋（Streak）で表現するスタイルの
/// 2 種類を切り替えて使用できます。ビームの方向、長さ、テクスチャのスクロール速度などを細かく設定可能です。
class LaserParticle : public Particle {
public:
  /// @brief 描画スタイル
  enum class Style {
    Segment, ///< ビームの軌道上に点を並べるスタイル
    Streak   ///< ビームの軌道に沿って筋を引き延ばすスタイル
  };

  /// @brief 初期化
  /// @param ctx シーンコンテキスト
  void Initialize(SceneContext &ctx) override;

  /// @brief ビームの基本パラメータを設定する
  /// @param start 始点
  /// @param end 終点
  /// @param width ビームの太さ
  /// @param life パーティクルの寿命
  /// @param color ビームの色
  void SetBeam(const Vector3 &start, const Vector3 &end, float width = 0.2f,
               float life = 0.05f, Vector4 color = {0.2f, 0.9f, 1.0f, 1.0f});

  /// @brief ビームの移動（スクロール）パラメータを設定する
  /// @param segmentSpacing セグメント間の間隔
  /// @param scrollSpeed 移動速度
  /// @param maxSegments 最大セグメント数
  void SetFlowParams(float segmentSpacing, float scrollSpeed,
                     int maxSegments = 64) {
    segmentSpacing_ = segmentSpacing;
    scrollSpeed_ = scrollSpeed;
    maxSegments_ = maxSegments;
  }

  /// @brief 描画スタイルを設定する
  /// @param s スタイル (Segment または Streak)
  void SetStyle(Style s) { style_ = s; }

  /// @brief ストリーク（縦筋）モードのパラメータ設定
  /// @param count ストリークの本数
  /// @param minLen ストリークの最小長さ
  /// @param maxLen ストリークの最大長さ
  /// @param streakWidth ストリークの横幅
  /// @param spreadZRatio 断面の奥行き散らし比率（推奨: 小さめの値）
  void SetStreakParams(int count, float minLen, float maxLen, float streakWidth,
                       float spreadZRatio = 0.15f) {
    streakCount_ = count;
    streakMinLen_ = minLen;
    streakMaxLen_ = maxLen;
    streakWidth_ = streakWidth;
    streakSpreadZRatio_ = spreadZRatio;
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

  /// @brief パーティクルのワールド行列構築（スタイルに応じたストレッチ処理を含む）
  void BuildWorldMatrix(const ParticleData &p,
                             const Matrix4x4 &billboardMatrix) const override;

  /// @brief フェード用アルファ計算
  float ComputeAlpha(const ParticleData &p) const override;

private:
  /// @brief ストリークモード時にパーティクルをビーム始点付近に再生成する
  void RespawnStreak(ParticleData &p);

private:
  float width_ = 0.2f; ///< ビーム全体の太さ
  float life_ = 0.05f; ///< パーティクルの寿命
  Vector4 color_ = {0.2f, 0.9f, 1.0f, 1.0f}; ///< ビームの色

  Vector3 beamStart_ = {0, 0, 0}; ///< ビーム始点
  Vector3 beamDir_ = {0, 1, 0};   ///< ビームの方向単位ベクトル
  float beamLen_ = 0.0f;          ///< ビームの長さ

  Vector3 beamRight_ = {1, 0, 0};   ///< ビーム断面の右方向基底ベクトル
  Vector3 beamForward_ = {0, 0, 1}; ///< ビーム断面の前方向基底ベクトル

  float segmentSpacing_ = 0.15f; ///< セグメント間隔
  float scrollSpeed_ = 6.0f;     ///< スクロール速度
  int maxSegments_ = 64;         ///< 最大セグメント数

  // ストリーク設定
  Style style_ = Style::Segment;   ///< 現在の描画スタイル
  int streakCount_ = 120;          ///< ストリークの本数
  float streakMinLen_ = 0.15f;     ///< 最小長
  float streakMaxLen_ = 0.8f;      ///< 最大長
  float streakWidth_ = 0.03f;      ///< 太さ
  float streakSpreadZRatio_ = 0.15f; ///< 奥行き方向への散らし率

  std::mt19937 rng_{1234567}; ///< 内部用乱数エンジン
};

} // namespace RC
