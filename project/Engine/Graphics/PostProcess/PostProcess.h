#pragma once
#include <cassert>
#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl/client.h>

class Dx12Core;
class PipelineManager;
class RenderTexture;
class GraphicsPipeline;

/// <summary>
/// ポストエフェクトの種類を表す
/// </summary>
enum class PostEffectType {
  None,       // そのまま転送（CopyImage）
  Grayscale,  // グレースケール
  Sepia,      // セピア調
  Vignette,   // ビネット（周辺減光）
  BoxFilter,  // ボックスフィルタ（平滑化）
};

/// <summary>
/// ポストプロセス（RenderTextureの内容を画面に描画する）を管理するクラス
/// エフェクトの重ね掛け（マルチパス）に対応
/// </summary>
class PostProcess {
public:
  PostProcess() = default;
  ~PostProcess() = default;

  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize(Dx12Core *dxCore, PipelineManager *pipelineManager,
                  uint32_t width, uint32_t height);

  /// <summary>
  /// 描画（マルチパス対応）
  /// </summary>
  void Draw(ID3D12GraphicsCommandList *cmdList,
            const RenderTexture &renderTexture);

  // ===========================
  // 単体エフェクト（後方互換）
  // ===========================

  /// <summary>
  /// ポストエフェクトを1つだけ設定する（スタックをクリアして設定）
  /// None を指定するとエフェクトなし
  /// </summary>
  void SetEffect(PostEffectType type);

  /// <summary>
  /// 先頭のエフェクトを返す（空なら None）
  /// </summary>
  PostEffectType GetEffect() const;

  // ===========================
  // 重ね掛け（スタック操作）
  // ===========================

  /// <summary>
  /// エフェクトをスタックに追加する（重複時は追加しない）
  /// </summary>
  void AddEffect(PostEffectType type);

  /// <summary>
  /// エフェクトをスタックから除去する
  /// </summary>
  void RemoveEffect(PostEffectType type);

  /// <summary>
  /// スタックを全クリアする
  /// </summary>
  void ClearEffects();

  /// <summary>
  /// 指定エフェクトがスタックに含まれているか
  /// </summary>
  bool HasEffect(PostEffectType type) const;

  /// <summary>
  /// 現在のエフェクトスタックを取得する
  /// </summary>
  const std::vector<PostEffectType> &GetEffects() const {
    return activeEffects_;
  }

  /// <summary>
  /// ポストエフェクトの ImGui を描画する
  /// </summary>
  void DrawImGui(const char *label = "PostEffect");

private:
  /// <summary>
  /// 1パス分のフルスクリーン描画
  /// </summary>
  void DrawSinglePass(ID3D12GraphicsCommandList *cmdList,
                      D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
                      GraphicsPipeline *pipeline,
                      PostEffectType effectType = PostEffectType::None);

  /// <summary>
  /// エフェクト種別に対応するパイプラインを返す
  /// </summary>
  GraphicsPipeline *GetPipelineForEffect(PostEffectType type);

private:
  Dx12Core *dxCore_ = nullptr;
  PipelineManager *pipelineManager_ = nullptr;

  uint32_t width_ = 0;
  uint32_t height_ = 0;

  // 各エフェクト用パイプライン
  GraphicsPipeline *pipelineCopy_ = nullptr;
  GraphicsPipeline *pipelineGrayscale_ = nullptr;
  GraphicsPipeline *pipelineSepia_ = nullptr;
  GraphicsPipeline *pipelineVignette_ = nullptr;
  GraphicsPipeline *pipelineBoxFilter_ = nullptr;

  // アクティブなエフェクトスタック（適用順）
  std::vector<PostEffectType> activeEffects_;

  // マルチパス用ピンポンテクスチャ（遅延初期化）
  std::unique_ptr<RenderTexture> pingPongA_;
  std::unique_ptr<RenderTexture> pingPongB_;
  bool pingPongInitialized_ = false;

  // BoxFilter パラメータ
  int boxFilterK_ = 1; // カーネル半径（1 = 3x3, 2 = 5x5, ...）

  void EnsurePingPongTextures();
};
