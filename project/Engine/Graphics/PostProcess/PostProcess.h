#pragma once
#include <cassert>
#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include "Dx12/SRVManager/SRVManager.h"

class Dx12Core;
class PipelineManager;
class RenderTexture;
class GraphicsPipeline;

/// @brief ポストエフェクトの種類を定義する列挙型
enum class PostEffectType {
  None,       ///< そのまま転送（CopyImage）
  Grayscale,  ///< グレースケール
  Sepia,      ///< セピア調
  Vignette,   ///< ビネット（周辺減光）
  BoxFilter,  ///< ボックスフィルタ（平滑化）
  DepthBasedOutline, ///< 深度ベースアウトライン
};

/// @class PostProcess
/// @brief ポストプロセス（RenderTextureの内容を画面に描画する）を管理するクラス
/// @details エフェクトの重ね掛け（マルチパス）に対応しており、ピンポンバッファを用いて複数のエフェクトを順次適用できます。
class PostProcess {
public:
  PostProcess() = default;
  ~PostProcess() = default;

  /// @brief 初期化
  /// @param dxCore DirectX12コアシステムへのポインタ
  /// @param pipelineManager パイプライン管理クラスへのポインタ
  /// @param width 画面解像度（幅）
  /// @param height 画面解像度（高さ）
  void Initialize(Dx12Core *dxCore, PipelineManager *pipelineManager,
                  uint32_t width, uint32_t height);

  /// @brief 描画実行（スタックされた全エフェクトを適用）
  /// @param cmdList コマンドリスト
  /// @param renderTexture 描画ソースとなるレンダーテクスチャ
  void Draw(ID3D12GraphicsCommandList *cmdList,
            const RenderTexture &renderTexture);

  /// @brief プロジェクション逆行列を設定する (DepthBasedOutline等で使用)
  void SetProjectionInverse(const float* projInv16);

  // ===========================
  // 単体エフェクト（後方互換）
  // ===========================

  /// @brief アウトラインの色を設定する
  void SetOutlineColor(const float color[4]);

  /// @brief アウトラインの太さ（検出の重み）を設定する
  void SetOutlineWeight(float weight);

  /// @brief アウトラインのピクセル幅（太さ）を設定する
  void SetOutlineThickness(float thickness);

  /// @brief アウトラインの描画モード（0:両側, 1:外側, 2:内側）を設定する
  void SetOutlineMode(int mode);

  /// @brief ポストエフェクトを1つだけ設定する
  /// @details 既存のエフェクトスタックをクリアして、指定されたエフェクトのみを設定します。
  /// @param type 設定するエフェクトの種類（None でエフェクトなし）
  void SetEffect(PostEffectType type);

  /// @brief 現在適用されている先頭のエフェクトを取得する
  /// @return エフェクトの種類（スタックが空なら None）
  PostEffectType GetEffect() const;

  // ===========================
  // 重ね掛け（スタック操作）
  // ===========================

  /// @brief エフェクトを適用スタックに追加する
  /// @note すでに同じ種類のエフェクトが含まれている場合は追加しません。
  /// @param type 追加するエフェクトの種類
  void AddEffect(PostEffectType type);

  /// @brief エフェクトを適用スタックから除去する
  /// @param type 除去するエフェクトの種類
  void RemoveEffect(PostEffectType type);

  /// @brief 全てのエフェクトを解除する
  void ClearEffects();

  /// @brief 指定したエフェクトが現在有効かどうかを確認する
  /// @param type 確認するエフェクトの種類
  /// @return 有効なら true
  bool HasEffect(PostEffectType type) const;

  /// @brief 現在有効なエフェクトのリストを取得する
  /// @return 適用順に並んだエフェクトの配列
  const std::vector<PostEffectType> &GetEffects() const {
    return activeEffects_;
  }

  /// @brief ImGui を使用したデバッグ用 UI を表示する
  /// @param label ウィンドウまたはヘッダーのラベル名
  void DrawImGui(const char *label = "PostEffect");

private:
  /// @brief 1パス分のフルスクリーン描画を実行する
  /// @param cmdList コマンドリスト
  /// @param srcSRV 描画元テクスチャの SRV ハンドル
  /// @param pipeline 使用するグラフィックスパイプライン
  /// @param effectType エフェクトの種類（定数バッファ等の切り替え用）
  void DrawSinglePass(ID3D12GraphicsCommandList *cmdList,
                      D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
                      GraphicsPipeline *pipeline,
                      PostEffectType effectType = PostEffectType::None);

  /// @brief エフェクト種別に対応するパイプラインを取得する
  /// @param type エフェクトの種類
  /// @return 対応するパイプラインへのポインタ（未登録なら nullptr）
  GraphicsPipeline *GetPipelineForEffect(PostEffectType type);

  /// @brief マルチパス用のピンポンテクスチャが必要な時に生成されることを保証する
  void EnsurePingPongTextures();

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
  GraphicsPipeline *pipelineDepthBasedOutline_ = nullptr;

  std::vector<PostEffectType> activeEffects_; ///< アクティブなエフェクトスタック（適用順）

  // CBuffer (DepthBasedOutline等で使用)
  Microsoft::WRL::ComPtr<ID3D12Resource> cbufferMaterial_;
  struct MaterialData {
    float projectionInverse[16];
    float outlineColor[4];
    float outlineWeight;
    float outlineThickness;
    int outlineMode;
    float padding;
  };
  MaterialData* mappedMaterial_ = nullptr;
  SRVManager::Handle depthSrv_{};

  float outlineColor_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float outlineWeight_ = 1.0f;
  float outlineThickness_ = 1.0f;
  int outlineMode_ = 0;

  // マルチパス用ピンポンテクスチャ
  std::unique_ptr<RenderTexture> pingPongA_;
  std::unique_ptr<RenderTexture> pingPongB_;
  bool pingPongInitialized_ = false;

  int boxFilterK_ = 1; ///< BoxFilter のカーネル半径（1 = 3x3, 2 = 5x5, ...）
};
