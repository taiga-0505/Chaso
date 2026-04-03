#include "HitEffect.h"
#include "Dx12/Dx12Core.h"
#include "Dx12/SwapChain/SwapChain.h"
#include "Mesh/MeshGenerator.h"
#include <Math/Math.h>
#include <algorithm>

namespace RC {

void HitEffect::Initialize(SceneContext &ctx) {
  // 1. 閃光パーティクル作成
  sparks_ = std::make_unique<FlashParticle>();
  sparks_->Initialize(ctx);
  sparks_->SetEmitterAutoSpawn(false);

  // 2. リングパーティクル作成
  ring_ = std::make_unique<RingParticle>();
  ring_->Initialize(ctx);
}

void HitEffect::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  // パーティクルの更新
  sparks_->GetEmitter().timeScale = timeScale_;
  sparks_->Update(view, proj);

  if (ring_) {
    ring_->GetEmitter().timeScale = timeScale_;
    ring_->Update(view, proj);
  }
}

void HitEffect::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  if (sparks_) {
    sparks_->Render(ctx, cl);
  }
  if (ring_) {
    ring_->Render(ctx, cl);
  }
}

void HitEffect::Trigger(const Vector3 &pos, const Vector4 &color) {
  // パーティクル: 閃光を放出
  sparks_->SetEmitterTranslation(pos);
  sparks_->SetEmitterColor(color);
  sparks_->SetEmitterScale({globalScale_, globalScale_, 1.05f});
  sparks_->GetEmitter().count = 12;
  sparks_->AddParticlesFromEmitter();

  // リング: 放出
  if (ring_) {
    ring_->SetEmitterTranslation(pos);
    ring_->SetEmitterColor(color);
    ring_->SetEmitterScale({globalScale_, globalScale_, globalScale_});
    ring_->AddParticlesFromEmitter();
  }
}

} // namespace RC
