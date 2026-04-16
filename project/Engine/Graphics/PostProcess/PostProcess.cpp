#include "PostProcess.h"
#include "Dx12/Dx12Core.h"
#include "Dx12/GraphicsPipeline/GraphicsPipeline.h"
#include "Dx12/PipelineManager.h"
#include "Graphics/Texture/RenderTexture/RenderTexture.h"

void PostProcess::Initialize(Dx12Core *dxCore,
                             PipelineManager *pipelineManager) {
  dxCore_ = dxCore;
  pipelineManager_ = pipelineManager;

  // PipelineManager に追加した "copyimage" を取得
  pipeline_ = pipelineManager_->Get("copyimage.none");
  assert(pipeline_ && "Failed to get copyimage pipeline");
}

void PostProcess::Draw(ID3D12GraphicsCommandList *cmdList,
                       const RenderTexture &renderTexture) {
  assert(pipeline_);

  // パイプライン設定
  cmdList->SetGraphicsRootSignature(pipeline_->Root());
  cmdList->SetPipelineState(pipeline_->PSO());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // t0: Texture (RenderTexture の SRV)
  // RootSignatureType::PostProcess では params[0] が SRV table (t0)
  cmdList->SetGraphicsRootDescriptorTable(0, renderTexture.GetSRVGPU());

  // 全画面三角形の描画（頂点バッファなし、SV_VertexIDを使用）
  cmdList->DrawInstanced(3, 1, 0, 0);
}
