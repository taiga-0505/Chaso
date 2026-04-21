#include "PostProcess.h"
#include "Dx12/Dx12Core.h"
#include "Dx12/GraphicsPipeline/GraphicsPipeline.h"
#include "Dx12/PipelineManager.h"
#include "Graphics/Texture/RenderTexture/RenderTexture.h"
#include "Common/Log/Log.h"

#include <algorithm>
#include <format>

namespace {
const char* ToString(PostEffectType type) {
  switch (type) {
  case PostEffectType::Grayscale: return "Grayscale";
  case PostEffectType::Sepia:     return "Sepia";
  case PostEffectType::Vignette:  return "Vignette";
  case PostEffectType::None:      return "None";
  default:                        return "Unknown";
  }
}

std::string ActiveEffectsToString(const std::vector<PostEffectType>& effects) {
  if (effects.empty()) return "None";
  std::string result;
  for (size_t i = 0; i < effects.size(); ++i) {
    if (i > 0) result += " -> ";
    result += ToString(effects[i]);
  }
  return result;
}
} // namespace

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

void PostProcess::Initialize(Dx12Core *dxCore,
                             PipelineManager *pipelineManager,
                             uint32_t width, uint32_t height) {
  dxCore_ = dxCore;
  pipelineManager_ = pipelineManager;
  width_ = width;
  height_ = height;

  // 各パイプラインを取得
  pipelineCopy_ = pipelineManager_->Get("copyimage.none");
  assert(pipelineCopy_ && "Failed to get copyimage pipeline");

  pipelineGrayscale_ = pipelineManager_->Get("grayscale.none");
  assert(pipelineGrayscale_ && "Failed to get grayscale pipeline");

  pipelineSepia_ = pipelineManager_->Get("sepia.none");
  assert(pipelineSepia_ && "Failed to get sepia pipeline");

  pipelineVignette_ = pipelineManager_->Get("vignette.none");
  assert(pipelineVignette_ && "Failed to get vignette pipeline");
}

// ============================================================================
// エフェクト操作
// ============================================================================

void PostProcess::SetEffect(PostEffectType type) {
  activeEffects_.clear();
  if (type != PostEffectType::None) {
    activeEffects_.push_back(type);
    Log::Print(std::format("[PostProcess] SetEffect: {} (Active: {})", ToString(type), ActiveEffectsToString(activeEffects_)));
  } else {
    Log::Print(std::format("[PostProcess] SetEffect: None (Active: {})", ActiveEffectsToString(activeEffects_)));
  }
}

PostEffectType PostProcess::GetEffect() const {
  return activeEffects_.empty() ? PostEffectType::None : activeEffects_.front();
}

void PostProcess::AddEffect(PostEffectType type) {
  if (type == PostEffectType::None) {
    return;
  }
  if (!HasEffect(type)) {
    activeEffects_.push_back(type);
    Log::Print(std::format("[PostProcess] AddEffect: {} (Active: {})", ToString(type), ActiveEffectsToString(activeEffects_)));
  }
}

void PostProcess::RemoveEffect(PostEffectType type) {
  auto it = std::find(activeEffects_.begin(), activeEffects_.end(), type);
  if (it != activeEffects_.end()) {
    activeEffects_.erase(it);
    Log::Print(std::format("[PostProcess] RemoveEffect: {} (Active: {})", ToString(type), ActiveEffectsToString(activeEffects_)));
  }
}

void PostProcess::ClearEffects() { 
  if (!activeEffects_.empty()) {
    activeEffects_.clear();
    Log::Print(std::format("[PostProcess] ClearEffects (Active: {})", ActiveEffectsToString(activeEffects_)));
  }
}

bool PostProcess::HasEffect(PostEffectType type) const {
  return std::find(activeEffects_.begin(), activeEffects_.end(), type) !=
         activeEffects_.end();
}

// ============================================================================
// パイプライン選択
// ============================================================================

GraphicsPipeline *PostProcess::GetPipelineForEffect(PostEffectType type) {
  switch (type) {
  case PostEffectType::Grayscale:
    return pipelineGrayscale_;
  case PostEffectType::Sepia:
    return pipelineSepia_;
  case PostEffectType::Vignette:
    return pipelineVignette_;
  case PostEffectType::None:
  default:
    return pipelineCopy_;
  }
}

// ============================================================================
// ピンポンテクスチャ（マルチパス用・遅延初期化）
// ============================================================================

void PostProcess::EnsurePingPongTextures() {
  if (pingPongInitialized_) {
    return;
  }
  pingPongA_ = std::make_unique<RenderTexture>();
  pingPongA_->Initialize(dxCore_, width_, height_);

  pingPongB_ = std::make_unique<RenderTexture>();
  pingPongB_->Initialize(dxCore_, width_, height_);

  pingPongInitialized_ = true;
}

// ============================================================================
// 1パス描画
// ============================================================================

void PostProcess::DrawSinglePass(ID3D12GraphicsCommandList *cmdList,
                                 D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
                                 GraphicsPipeline *pipeline) {
  assert(pipeline);
  cmdList->SetGraphicsRootSignature(pipeline->Root());
  cmdList->SetPipelineState(pipeline->PSO());
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootSignatureType::PostProcess → params[0] が SRV table (t0)
  cmdList->SetGraphicsRootDescriptorTable(0, srcSRV);

  // 全画面三角形（頂点バッファなし、SV_VertexID 使用）
  cmdList->DrawInstanced(3, 1, 0, 0);
}

// ============================================================================
// メイン描画（マルチパス対応）
// ============================================================================

void PostProcess::Draw(ID3D12GraphicsCommandList *cmdList,
                       const RenderTexture &renderTexture) {
  // --- エフェクトなし：そのままコピー ---
  if (activeEffects_.empty()) {
    DrawSinglePass(cmdList, renderTexture.GetSRVGPU(), pipelineCopy_);
    return;
  }

  // --- 1エフェクト：シングルパス ---
  if (activeEffects_.size() == 1) {
    DrawSinglePass(cmdList, renderTexture.GetSRVGPU(),
                   GetPipelineForEffect(activeEffects_[0]));
    return;
  }

  // --- 2エフェクト以上：マルチパス（ピンポン） ---
  EnsurePingPongTextures();

  RenderTexture *textures[2] = {pingPongA_.get(), pingPongB_.get()};
  const size_t count = activeEffects_.size();

  // ビューポート・シザー（全パス共通サイズ）
  D3D12_VIEWPORT vp{};
  vp.Width = static_cast<float>(width_);
  vp.Height = static_cast<float>(height_);
  vp.MaxDepth = 1.0f;
  D3D12_RECT sr{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};

  for (size_t i = 0; i < count; ++i) {
    const bool isFirst = (i == 0);
    const bool isLast = (i == count - 1);

    // ── ソースSRV ──
    D3D12_GPU_DESCRIPTOR_HANDLE srcSRV;
    if (isFirst) {
      // 最初のパス：入力テクスチャ（App の RenderTexture、すでに SRV 状態）
      srcSRV = renderTexture.GetSRVGPU();
    } else {
      // 前パスの出力をソースにする
      RenderTexture *prevDst = textures[(i - 1) % 2];
      prevDst->TransitionToShaderResource(cmdList);
      srcSRV = prevDst->GetSRVGPU();
    }

    // ── デスティネーション ──
    if (!isLast) {
      // 中間パス：ピンポンテクスチャに書き込む
      RenderTexture *dst = textures[i % 2];
      dst->TransitionToRenderTarget(cmdList);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv = dst->GetRTV();
      cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      cmdList->RSSetViewports(1, &vp);
      cmdList->RSSetScissorRects(1, &sr);
    } else {
      // 最終パス：バックバッファに書き込む（App が事前に設定済み）
      // 中間パスでRTを変更しているので再設定する
      D3D12_CPU_DESCRIPTOR_HANDLE backRtv = dxCore_->CurrentRTV();
      cmdList->OMSetRenderTargets(1, &backRtv, FALSE, nullptr);
      dxCore_->ResetViewportScissorToBackbuffer(width_, height_);
    }

    // ── 描画 ──
    DrawSinglePass(cmdList, srcSRV,
                   GetPipelineForEffect(activeEffects_[i]));
  }
}

// ============================================================================
// ImGui
// ============================================================================

void PostProcess::DrawImGui([[maybe_unused]] const char *label) {
#if RC_ENABLE_IMGUI
  if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
    // 各エフェクトのチェックボックス
    bool grayscale = HasEffect(PostEffectType::Grayscale);
    bool sepia = HasEffect(PostEffectType::Sepia);
    bool vignette = HasEffect(PostEffectType::Vignette);

    if (ImGui::Checkbox("Grayscale", &grayscale)) {
      if (grayscale) {
        AddEffect(PostEffectType::Grayscale);
      } else {
        RemoveEffect(PostEffectType::Grayscale);
      }
    }

    if (ImGui::Checkbox("Sepia", &sepia)) {
      if (sepia) {
        AddEffect(PostEffectType::Sepia);
      } else {
        RemoveEffect(PostEffectType::Sepia);
      }
    }

    if (ImGui::Checkbox("Vignette", &vignette)) {
      if (vignette) {
        AddEffect(PostEffectType::Vignette);
      } else {
        RemoveEffect(PostEffectType::Vignette);
      }
    }

    // 現在のスタック表示
    const auto &effects = GetEffects();
    if (effects.empty()) {
      ImGui::TextDisabled("Active: None");
    } else {
      ImGui::Text("Active: %zu effect(s)", effects.size());
    }
  }
#endif
}
