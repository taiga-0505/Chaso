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
#include <functional>
#include <memory>
#include <string_view>
#include <vector>
#include <wrl.h>
#include <future>
#include <mutex>

#include "RenderQueue.h"
#include "FrameResource.h"


#include "Light/Area/AreaLightManager.h"
#include "Light/Directional/DirectionalLightManager.h"
#include "Light/Point/PointLightManager.h"
#include "Light/Spot/SpotLightManager.h"
#include "Model/ModelManager.h"
#include "Skydome/SkydomeManager.h"
#include "Skybox/SkyboxManager.h"
#include "Mesh/PrimitiveMeshManager.h"
#include "Sprite/SpriteManager.h"
#include "Texture/TextureManager/TextureManager.h"

#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode
#include "Math/Math.h"
#include "Model/ModelObject.h" // ModelManager の unique_ptr<ModelObject> に必要
#include "Graphics/Skydome/Skydome.h"    // SkydomeManager の unique_ptr<Skydome> に必要
#include "Graphics/Skybox/Skybox.h"      // SkyboxManager の unique_ptr<Skybox> に必要
#include "Graphics/Mesh/PrimitiveMesh.h"
#include "Graphics/Mesh/MeshGenerator.h"
#include "function/function.h"
#include "struct.h"

class Primitive2D;
class Primitive3D;
class GraphicsPipeline;
class DescriptorHeap;
class PostProcess;
struct SceneContext;

namespace RC {

class RenderContext {
public:
  // ── シングルトン ────────────────────────────────────
  static RenderContext &GetInstance();

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

  // ── 描画パス実行 ───────────────────────────────────
  void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);
  void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  // ── 表示モード ─────────────────────────────────────
  void SetViewShadingMode(ViewShadingMode mode) { viewShadingMode_ = mode; }
  ViewShadingMode GetViewShadingMode() const { return viewShadingMode_; }

  // ── テクスチャヘルパー ─────────────────────────────
  int LoadTex(const std::string &path, bool srgb);
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle);

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
  SkydomeManager &Skydomes() { return skydomeMan_; }
  SkyboxManager &SkyBoxes() { return skyboxMan_; }
  PrimitiveMeshManager &PrimitiveMeshes() { return primitiveMeshMan_; }
  DirectionalLightManager &DirLights() { return dirLightMan_; }
  PointLightManager &PtLights() { return ptLightMan_; }
  SpotLightManager &SpLights() { return spLightMan_; }
  AreaLightManager &ArLights() { return arLightMan_; }
  TextureManager &Textures() { return texMan_; }

  // ── PostProcess アクセス ─────────────────────────────
  PostProcess *GetPostProcess() const { return postProcess_; }

  // ── PSO バインドヘルパー ───────────────────────────
  GraphicsPipeline *BindPipeline(std::string_view prefix);
  GraphicsPipeline *GetPipeline(std::string_view prefix, BlendMode mode);
  void BindCameraCB();
  void BindAllLightCBs();

  // ── Primitive 遅延生成 ─────────────────────────────
  Primitive2D *EnsurePrimitive2D();
  Primitive3D *EnsurePrimitive3D();

  // ── 非同期ロード管理 ──────────────────────────────
  void WaitAllLoads();
  void AddLoadingTask(std::future<void> &&task);

  // ── コマンドキュー ─────────────────────────────────
  struct RenderCommand3D {
    enum Type {
      Other,
      Primitive,
    } type = Other;

    uint64_t sortKey = 0; // 0 = ソートなし（push順を維持）

    std::function<void(ID3D12GraphicsCommandList *)> func;

    // Primitive マージ用の情報
    bool primDepth = false;
    uint32_t primStart = 0;
    uint32_t primCount = 0;
  };

  void PushCommand3D(std::function<void(ID3D12GraphicsCommandList *)> func) {
    RenderCommand3D cmd;
    cmd.type = RenderCommand3D::Other;
    cmd.func = std::move(func);
    commandQueue3D_.push_back(std::move(cmd));
  }

  /// ソートキー付きでコマンドを積む（同じキューでstable_sort）
  void PushCommand3D(uint64_t sortKey,
                     std::function<void(ID3D12GraphicsCommandList *)> func) {
    RenderCommand3D cmd;
    cmd.type = RenderCommand3D::Other;
    cmd.sortKey = sortKey;
    cmd.func = std::move(func);
    commandQueue3D_.push_back(std::move(cmd));
  }

  void PushPrimitive3DCommand(bool depth, uint32_t start, uint32_t count);
  void Execute3DCommands();
  void Clear3DCommands() { commandQueue3D_.clear(); }

  // ── FrameResource（フレームごとのリニアアロケータ）──
  FrameResource &CurrentFrame() { return frameResources_[frameIndex_]; }
  uint32_t FrameIndex() const { return frameIndex_; }
  void AdvanceFrame() { frameIndex_ = (frameIndex_ + 1) % FrameResource::kFrameCount; }


  // ── Fog CB ─────────────────────────────────────────

  void UpdateFogCB(float timeSec, float intensity, float scale, float speed,
                   const Vector2 &wind, float feather, float bottomBias);
  void SetFogColor(const Vector4 &color);
  ID3D12Resource *FogCBResource() const { return fogCB_.Get(); }

private:
  bool initialized_ = false;

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  DescriptorHeap *srvHeap_ = nullptr;
  ID3D12GraphicsCommandList *cl_ = nullptr;
  SceneContext *ctxRef_ = nullptr;
  PostProcess *postProcess_ = nullptr;

  Matrix4x4 view_;
  Matrix4x4 proj_;
  BlendMode currentBlendMode_ = kBlendModeNone;
  ViewShadingMode viewShadingMode_ = ViewShadingMode::Solid;

  // マネージャー群
  ModelManager modelMan_;
  SpriteManager spriteMan_;
  SkydomeManager skydomeMan_;
  SkyboxManager skyboxMan_;
  PrimitiveMeshManager primitiveMeshMan_;
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

  // コマンドキュー（sortKey によるステーブルソート対応）
  std::vector<RenderCommand3D> commandQueue3D_;

  // FrameResource（ダブル/トリプルバッファリング）
  std::array<FrameResource, FrameResource::kFrameCount> frameResources_;
  uint32_t frameIndex_ = 0;

  // 非同期タスク管理
  std::vector<std::future<void>> ongoingTasks_;
  std::mutex mtxTasks_;

  // リソース生成同期用
  std::mutex mtxResource_;

public:
  std::mutex &ResourceMutex() { return mtxResource_; }
};

RenderContext &GetRenderContext();

} // namespace RC
