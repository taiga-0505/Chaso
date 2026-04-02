#include "RenderContext.h"

#include "Dx12/Dx12Core.h"
#include "PipelineManager.h"
#include "Primitive2D/Primitive2D.h"
#include "Primitive3D/Primitive3D.h"
#include "Scene.h"

namespace RC {

// ============================================================================
// Init / Term
// ============================================================================

void RenderContext::Init(SceneContext &ctx) {
  if (initialized_) {
    return;
  }

  ctxRef_ = &ctx;
  device_ = ctx.core->GetDevice();
  srvHeap_ = &ctx.core->SRV();

  texMan_.Init(&ctx.core->SRVMan());
  spriteMan_.Init(device_.Get(), &texMan_);
  modelMan_.Init(device_.Get(), &texMan_);
  sphereMan_.Init(device_.Get(), &texMan_);

  dirLightMan_.Init(device_.Get());
  ptLightMan_.Init(device_.Get());
  spLightMan_.Init(device_.Get());
  arLightMan_.Init(device_.Get());

  // CameraCB
  cameraCB_ = CreateBufferResource(device_.Get(), sizeof(CameraCB),
                                   L"RenderContext::CameraCB");
  cameraCB_->Map(0, nullptr, reinterpret_cast<void **>(&cameraCBMapped_));

  // FogOverlayCB
  fogCB_ = CreateBufferResource(device_.Get(), sizeof(FogOverlayCB),
                                L"RenderContext::FogCB");
  fogCB_->Map(0, nullptr, reinterpret_cast<void **>(&fogCBMapped_));
  if (fogCBMapped_) {
    *fogCBMapped_ = FogOverlayCB{};
  }

  view_ = MakeIdentity4x4();
  proj_ = MakeIdentity4x4();
  cl_ = nullptr;
  currentBlendMode_ = kBlendModeNone;

  initialized_ = true;
}

void RenderContext::Term() {
  if (!initialized_) {
    return;
  }

  modelMan_.Term();
  sphereMan_.Term();
  spriteMan_.Term();

  dirLightMan_.Term();
  ptLightMan_.Term();
  spLightMan_.Term();
  arLightMan_.Term();

  prim2D_.reset();
  prim3D_.reset();

  if (cameraCB_) {
    if (cameraCBMapped_) {
      cameraCB_->Unmap(0, nullptr);
      cameraCBMapped_ = nullptr;
    }
    cameraCB_.Reset();
  }

  if (fogCB_) {
    if (fogCBMapped_) {
      fogCB_->Unmap(0, nullptr);
      fogCBMapped_ = nullptr;
    }
    fogCB_.Reset();
  }

  texMan_.Term();

  ctxRef_ = nullptr;
  device_.Reset();
  srvHeap_ = nullptr;
  cl_ = nullptr;

  initialized_ = false;
}

// ============================================================================
// Camera
// ============================================================================

void RenderContext::SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
                              const Vector3 &camWorldPos) {
  view_ = view;
  proj_ = proj;

  if (cameraCBMapped_) {
    cameraCBMapped_->worldPos = camWorldPos;
    cameraCBMapped_->_pad = 0.0f;
  }
}

// ============================================================================
// PSO ヘルパー
// ============================================================================

GraphicsPipeline *RenderContext::GetPipeline(std::string_view prefix,
                                             BlendMode mode) {
  if (!ctxRef_ || !ctxRef_->pipelineManager) {
    return nullptr;
  }

  GraphicsPipeline *pso =
      ctxRef_->pipelineManager->Get(PipelineManager::MakeKey(prefix, mode));

  if (!pso && mode != kBlendModeNormal) {
    pso = ctxRef_->pipelineManager->Get(
        PipelineManager::MakeKey(prefix, kBlendModeNormal));
  }

  return pso;
}

GraphicsPipeline *RenderContext::BindPipeline(std::string_view prefix) {
  if (!cl_ || !ctxRef_) {
    return nullptr;
  }

  auto *pso = GetPipeline(prefix, currentBlendMode_);
  if (!pso) {
    return nullptr;
  }

  cl_->SetGraphicsRootSignature(pso->Root());
  cl_->SetPipelineState(pso->PSO());
  cl_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  return pso;
}

void RenderContext::BindCameraCB() {
  if (!cl_ || !cameraCB_) {
    return;
  }
  cl_->SetGraphicsRootConstantBufferView(4,
                                         cameraCB_->GetGPUVirtualAddress());
}

void RenderContext::BindAllLightCBs() {
  if (!cl_) {
    return;
  }

  // PointLightCB → RootParam[5]
  if (const D3D12_GPU_VIRTUAL_ADDRESS addr = ptLightMan_.GetCBAddress()) {
    cl_->SetGraphicsRootConstantBufferView(5, addr);
  }
  // SpotLightCB → RootParam[6]
  if (const D3D12_GPU_VIRTUAL_ADDRESS addr = spLightMan_.GetCBAddress()) {
    cl_->SetGraphicsRootConstantBufferView(6, addr);
  }
  // AreaLightCB → RootParam[7]
  if (const D3D12_GPU_VIRTUAL_ADDRESS addr = arLightMan_.GetCBAddress()) {
    cl_->SetGraphicsRootConstantBufferView(7, addr);
  }
}

// ============================================================================
// Primitive 遅延生成
// ============================================================================

Primitive2D *RenderContext::EnsurePrimitive2D() {
  if (!initialized_ || !device_ || !ctxRef_ || !ctxRef_->app) {
    return nullptr;
  }

  const float w = static_cast<float>(ctxRef_->app->width);
  const float h = static_cast<float>(ctxRef_->app->height);

  if (!prim2D_) {
    prim2D_ = std::make_unique<Primitive2D>();
    prim2D_->Initialize(device_.Get(), w, h);
  } else {
    prim2D_->SetScreenSize(w, h);
  }

  return prim2D_.get();
}

Primitive3D *RenderContext::EnsurePrimitive3D() {
  if (!initialized_ || !device_ || !ctxRef_) {
    return nullptr;
  }

  if (!prim3D_) {
    prim3D_ = std::make_unique<Primitive3D>();
    prim3D_->Initialize(device_.Get());
  }
  return prim3D_.get();
}

// ============================================================================
// Fog CB
// ============================================================================

void RenderContext::UpdateFogCB(float timeSec, float intensity, float scale,
                                float speed, const Vector2 &wind,
                                float feather, float bottomBias) {
  if (fogCBMapped_) {
    fogCBMapped_->timeSec = timeSec;
    fogCBMapped_->intensity = intensity;
    fogCBMapped_->scale = scale;
    fogCBMapped_->speed = speed;
    fogCBMapped_->wind = wind;
    fogCBMapped_->feather = feather;
    fogCBMapped_->bottomBias = bottomBias;
  }
}

void RenderContext::SetFogColor(const Vector4 &color) {
  if (fogCBMapped_) {
    fogCBMapped_->color = color;
  }
}

} // namespace RC
