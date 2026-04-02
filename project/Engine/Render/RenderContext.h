#pragma once

// ============================================================================
// RenderContext
// ----------------------------------------------------------------------------
// エンジン内部専用。描画サブシステム全体の状態を保持するクラス。
// RenderCommon.cpp が唯一のインスタンスを所有し、各サブモジュール
// (RenderModel.cpp 等) は GetRenderContext() 経由で参照する。
//
// ★ このヘッダーをシーン側 (#include "RenderCommon.h" のみ) が直接
//    include することはない。
// ============================================================================

#include <d3d12.h>
#include <memory>
#include <string_view>
#include <wrl.h>

#include "Light/Area/AreaLightManager.h"
#include "Light/Directional/DirectionalLightManager.h"
#include "Light/Point/PointLightManager.h"
#include "Light/Spot/SpotLightManager.h"
#include "Model3D/ModelManager.h"
#include "Sphere/SphereManager.h"
#include "Sprite2D/SpriteManager.h"
#include "Texture/TextureManager/TextureManager.h"

#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode
#include "Math/Math.h"
#include "Model3d/ModelObject.h" // ModelManager の unique_ptr<ModelObject> に必要
#include "Sphere/Sphere.h"      // SphereManager の unique_ptr<Sphere> に必要
#include "function/function.h"
#include "struct.h"

class Primitive2D;
class Primitive3D;
class GraphicsPipeline;
class DescriptorHeap;
struct SceneContext;

namespace RC {

class RenderContext {
public:
  // ── 初期化 / 終了 ──────────────────────────────────
  void Init(SceneContext &ctx);
  void Term();
  bool IsInitialized() const { return initialized_; }

  // ── フレーム状態の更新 ─────────────────────────────
  void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
                 const Vector3 &camWorldPos);
  void SetCommandList(ID3D12GraphicsCommandList *cl) { cl_ = cl; }
  void SetSceneContext(SceneContext *ctx) { ctxRef_ = ctx; }
  void SetBlendMode(BlendMode mode) { currentBlendMode_ = mode; }

  // ── 読み取りアクセス（各サブモジュール用）──────────
  ID3D12GraphicsCommandList *CL() const { return cl_; }
  SceneContext *Ctx() const { return ctxRef_; }
  ID3D12Device *Device() const { return device_.Get(); }
  BlendMode CurrentBlendMode() const { return currentBlendMode_; }
  const Matrix4x4 &View() const { return view_; }
  const Matrix4x4 &Proj() const { return proj_; }

  // ── マネージャーアクセス ───────────────────────────
  ModelManager &Models() { return modelMan_; }
  SpriteManager &Sprites() { return spriteMan_; }
  SphereManager &Spheres() { return sphereMan_; }
  DirectionalLightManager &DirLights() { return dirLightMan_; }
  PointLightManager &PtLights() { return ptLightMan_; }
  SpotLightManager &SpLights() { return spLightMan_; }
  AreaLightManager &ArLights() { return arLightMan_; }
  TextureManager &Textures() { return texMan_; }

  // ── PSO バインドヘルパー ───────────────────────────
  /// prefix + 現在の BlendMode で PSO を選択しバインドする
  GraphicsPipeline *BindPipeline(std::string_view prefix);

  /// prefix + 指定 BlendMode で PSO を取得する（バインドはしない）
  GraphicsPipeline *GetPipeline(std::string_view prefix, BlendMode mode);

  /// CameraCB を RootParam[4] にバインドする
  void BindCameraCB();

  /// PointLight + SpotLight + AreaLight の CB を一括バインド
  void BindAllLightCBs();

  // ── Primitive 遅延生成 ─────────────────────────────
  Primitive2D *EnsurePrimitive2D();
  Primitive3D *EnsurePrimitive3D();

  // ── Fog CB ─────────────────────────────────────────
  void UpdateFogCB(float timeSec, float intensity, float scale, float speed,
                   const Vector2 &wind, float feather, float bottomBias);
  void SetFogColor(const Vector4 &color);
  ID3D12Resource *FogCBResource() const { return fogCB_.Get(); }

private:
  // ── 所有する状態（すべて private）──────────────────

  bool initialized_ = false;

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  DescriptorHeap *srvHeap_ = nullptr;
  ID3D12GraphicsCommandList *cl_ = nullptr;
  SceneContext *ctxRef_ = nullptr;

  Matrix4x4 view_;
  Matrix4x4 proj_;
  BlendMode currentBlendMode_ = kBlendModeNone;

  // マネージャー群
  ModelManager modelMan_;
  SpriteManager spriteMan_;
  SphereManager sphereMan_;
  DirectionalLightManager dirLightMan_;
  PointLightManager ptLightMan_;
  SpotLightManager spLightMan_;
  AreaLightManager arLightMan_;
  TextureManager texMan_;

  // Camera CB
  struct CameraCB {
    Vector3 worldPos;
    float _pad = 0.0f;
  };
  Microsoft::WRL::ComPtr<ID3D12Resource> cameraCB_;
  CameraCB *cameraCBMapped_ = nullptr;

  // Fog CB
  struct FogOverlayCB {
    float timeSec = 0.0f;
    float intensity = 0.25f;
    float scale = 4.0f;
    float speed = 0.05f;
    Vector2 wind = {0.08f, 0.03f};
    float feather = 0.18f;
    float bottomBias = 0.35f;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
  };
  Microsoft::WRL::ComPtr<ID3D12Resource> fogCB_;
  FogOverlayCB *fogCBMapped_ = nullptr;

  // Primitive
  std::unique_ptr<Primitive2D> prim2D_;
  std::unique_ptr<Primitive3D> prim3D_;
};

// ============================================================================
// GetRenderContext
// ----------------------------------------------------------------------------
// RenderCommon.cpp が保持する唯一のインスタンスへの参照を返す。
// 各サブモジュール (RenderModel.cpp 等) はこれを使ってアクセスする。
// ============================================================================
RenderContext &GetRenderContext();

} // namespace RC
