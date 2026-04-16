#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Dx12Core;
class PipelineManager;
class RenderTexture;
class GraphicsPipeline;

/// <summary>
/// ポストプロセス（RenderTextureの内容を画面に描画する）を管理するクラス
/// </summary>
class PostProcess {
public:
  PostProcess() = default;
  ~PostProcess() = default;

  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize(Dx12Core *dxCore, PipelineManager *pipelineManager);

  /// <summary>
  /// 描画
  /// </summary>
  /// <param name="cmdList">コマンドリスト</param>
  /// <param name="renderTexture">転送元の RenderTexture</param>
  void Draw(ID3D12GraphicsCommandList *cmdList, const RenderTexture &renderTexture);

private:
  Dx12Core *dxCore_ = nullptr;
  PipelineManager *pipelineManager_ = nullptr;
  GraphicsPipeline *pipeline_ = nullptr;
};
