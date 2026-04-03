#pragma once
#include "Graphics/Mesh/PrimitiveMesh.h"
#include "Particle/Flash/FlashParticle.h"
#include "Particle/Ring/RingParticle.h"
#include "Scene.h"
#include <memory>

namespace RC {

/// <summary>
/// 資料(Reference)に基づいたヒットエフェクト
/// - 地面に広がるリング (RingParticle: GenerateRingEx)
/// - 飛び散る閃光 (FlashParticle)
/// </summary>
class HitEffect {
public:
  void Initialize(SceneContext &ctx);
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  // 指定位置でエフェクトを一発発生させる
  void Trigger(const Vector3 &pos, const Vector4 &color = {1, 1, 1, 1});

  // 一括制御パラメータ設定
  void SetGlobalScale(float s) { globalScale_ = s; }
  void SetTimeScale(float t) { timeScale_ = t; }

private:
  // 閃光パーティクル
  std::unique_ptr<FlashParticle> sparks_;
  // リングパーティクル
  std::unique_ptr<RingParticle> ring_;

  float globalScale_ = 1.0f;
  float timeScale_ = 1.0f;
};

} // namespace RC
