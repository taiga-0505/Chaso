#include "RenderCommon.h"

#include "Dx12/Dx12Core.h"
#include "Light/Area/AreaLightManager.h"
#include "Light/Directional/DirectionalLightManager.h"
#include "Light/Point/PointLightManager.h"
#include "Light/Spot/SpotLightManager.h"
#include "Model3D/ModelManager.h"
#include "Model3d/ModelObject.h"
#include "PipelineManager.h"
#include "Primitive2D/Primitive2D.h"
#include "Primitive3D/Primitive3D.h"
#include "Sphere/Sphere.h"
#include "Sphere/SphereManager.h"
#include "Sprite2D/SpriteManager.h"
#include "Texture/TextureManager/TextureManager.h"

#include "struct.h"
#include <algorithm>
#include <memory>
#include <string_view>

namespace RC {

// ============================================================================
// このファイルの目的
// ----------------------------------------------------------------------------
// RenderCommon は「Scene から呼べる描画ファサード。」
// - ハンドル制で Model / Sprite / Sphere / Light を管理
// - そのフレームのカメラ(View/Proj + カメラ位置)を共有
// - 今フレームの CommandList / SceneContext を保持して Draw* へ渡す
// - PipelineManager から prefix + BlendMode で PSO を選んでセットする
//
// 重要な前提:
// - スレッドセーフではない（基本はメインスレッド想定）
// - Draw* は「直前に PreDraw3D / PreDraw2D が呼ばれている」前提
//   （呼ばれていない/失敗してる場合は gCL が nullptr になり Draw* は無視）
// - RootSignature の RootParam index は固定値で埋め込まれている
//   (例: CameraCB は RootParam[4])
//   => ここを変える時はシェーダー/ルートシグネチャと必ず同期させること
// ============================================================================

namespace {

// ============================================================================
// CameraCB
// ----------------------------------------------------------------------------
// Camera の「ワールド空間座標」だけを GPU に送る用。
// - View/Proj は各オブジェクト Update で渡してるので CB に入れていない。
// - 16バイトアラインに合わせるため pad を置く。
// ============================================================================
struct CameraCB {
  RC::Vector3 worldPos;
  float _pad = 0.0f; // 16B alignment
};

// ============================================================================
// FogOverlayCB
// ----------------------------------------------------------------------------
// 画面全体の白い霧（寒い表現）用。
// RootSignatureType::FogOverlay の RootParam[0] (b0 PS) に渡す。
// ============================================================================
struct FogOverlayCB {
  float timeSec = 0.0f;
  float intensity = 0.25f;
  float scale = 4.0f;
  float speed = 0.05f;

  RC::Vector2 wind = {0.08f, 0.03f};
  float feather = 0.18f;
  float bottomBias = 0.35f;

  RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct Primitive3DSlot {
  std::unique_ptr<Primitive3D> ptr;
  bool inUse = false;
};
static Primitive3DSlot gPrim3D;

struct PrimitiveSlot {
  std::unique_ptr<Primitive2D> ptr;
  bool inUse = false;
};

// ============================================================================
// グローバル状態（RenderCommon の "中の人"）
// ----------------------------------------------------------------------------
// - gCtxRef / gCL は PreDraw3D/PreDraw2D で毎フレーム更新
// - gView / gProj / gCameraCB は SetCamera で毎フレーム更新
// ============================================================================
RC::ModelManager gModelMan;
RC::SpriteManager gSpriteMan;
RC::SphereManager gSphereMan;
RC::DirectionalLightManager gLightMan;
RC::PointLightManager gPointLightMan;
RC::SpotLightManager gSpotLightMan;
RC::AreaLightManager gAreaLightMan;

ID3D12Device *gDevice = nullptr;
DescriptorHeap *gSrvHeap = nullptr;
TextureManager gTexMan;

Matrix4x4 gView;
Matrix4x4 gProj;

ID3D12GraphicsCommandList *gCL =
    nullptr;                     // Draw* が使う "今フレ" の CommandList
SceneContext *gCtxRef = nullptr; // PipelineManager にアクセスするために保持

static ID3D12Resource *gCameraCB = nullptr;
static CameraCB *gCameraCBMapped = nullptr;

static ID3D12Resource *gFogCB = nullptr;
static FogOverlayCB *gFogCBMapped = nullptr;

bool gInitialized = false;
static BlendMode gCurrentBlendMode = BlendMode::kBlendModeNone;

// ライトは LightManager に委譲
static PrimitiveSlot gPrim2D;

// ============================================================================
// Pipeline 選択
// ----------------------------------------------------------------------------
// prefix + BlendMode で PipelineManager から引いてくる。
// 見つからない場合は normal にフォールバック。
// - 例) prefix="sprite", mode=Add の PSO が無い時は sprite+Normal を使う
// ============================================================================
static GraphicsPipeline *GetPipeline_(SceneContext *ctx,
                                      std::string_view prefix, BlendMode mode) {
  if (!ctx || !ctx->pipelineManager) {
    return nullptr;
  }

  GraphicsPipeline *pso =
      ctx->pipelineManager->Get(PipelineManager::MakeKey(prefix, mode));

  if (!pso && mode != kBlendModeNormal) {
    pso = ctx->pipelineManager->Get(
        PipelineManager::MakeKey(prefix, kBlendModeNormal));
  }

  return pso;
}

// prefix + gCurrentBlendMode を使って PSO をセットする
static GraphicsPipeline *BindPipeline_(std::string_view prefix) {
  if (!gCL || !gCtxRef) {
    return nullptr;
  }

  auto *pso = GetPipeline_(gCtxRef, prefix, gCurrentBlendMode);
  if (!pso) {
    return nullptr;
  }

  gCL->SetGraphicsRootSignature(pso->Root());
  gCL->SetPipelineState(pso->PSO());
  gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  return pso;
}

// CameraCB は RootParam[4] にバインドされる想定
static void BindCameraCB_() {
  if (!gCL || !gCameraCB) {
    return;
  }
  gCL->SetGraphicsRootConstantBufferView(4, gCameraCB->GetGPUVirtualAddress());
}

// PointLightCB は RootParam[5]（b3）にバインドされる想定
static void BindPointLightCB_() {
  if (!gCL) {
    return;
  }
  const D3D12_GPU_VIRTUAL_ADDRESS addr = gPointLightMan.GetCBAddress();
  if (addr != 0) {
    gCL->SetGraphicsRootConstantBufferView(5, addr);
  }
}

// SpotLightCB は RootParam[6]（b4）にバインドされる想定
static void BindSpotLightCB_() {
  if (!gCL) {
    return;
  }
  const D3D12_GPU_VIRTUAL_ADDRESS addr = gSpotLightMan.GetCBAddress();
  if (addr != 0) {
    gCL->SetGraphicsRootConstantBufferView(6, addr);
  }
}

// AreaLightCB は RootParam[7]（b5）にバインドされる想定
static void BindAreaLightCB_() {
  if (!gCL) {
    return;
  }
  const D3D12_GPU_VIRTUAL_ADDRESS addr = gAreaLightMan.GetCBAddress();
  if (addr != 0) {
    gCL->SetGraphicsRootConstantBufferView(7, addr);
  }
}

// ============================================================================
// Primitive2D の遅延生成
// ----------------------------------------------------------------------------
// - PreDraw2D のタイミングで BeginFrame() を呼んで、即時描画を始める
// - 画面サイズが変わったら追従
// ============================================================================
static Primitive2D *EnsurePrimitive2D_() {
  if (!gInitialized || !gDevice || !gCtxRef || !gCtxRef->app) {
    return nullptr;
  }

  const float w = static_cast<float>(gCtxRef->app->width);
  const float h = static_cast<float>(gCtxRef->app->height);

  if (!gPrim2D.inUse || !gPrim2D.ptr) {
    gPrim2D.ptr = std::make_unique<Primitive2D>();
    gPrim2D.ptr->Initialize(gDevice, w, h);
    gPrim2D.inUse = true;
  } else {
    // リサイズ追従（毎フレーム呼んでも OK な想定）
    gPrim2D.ptr->SetScreenSize(w, h);
  }

  return gPrim2D.ptr.get();
}

static Primitive3D *EnsurePrimitive3D_() {
  if (!gInitialized || !gDevice || !gCtxRef)
    return nullptr;

  if (!gPrim3D.inUse || !gPrim3D.ptr) {
    gPrim3D.ptr = std::make_unique<Primitive3D>();
    gPrim3D.ptr->Initialize(gDevice);
    gPrim3D.inUse = true;
  }
  return gPrim3D.ptr.get();
}

} // namespace

// ============================================================================
// 公開 API
// ============================================================================

// ----------------------------------------------------------------------------
// Init / Term
// ----------------------------------------------------------------------------

void Init(SceneContext &ctx) {
  if (gInitialized) {
    return;
  }

  gCtxRef = &ctx;
  gDevice = ctx.core->GetDevice();
  gSrvHeap = &ctx.core->SRV();

  // TextureManager は SRVManager を使ってハンドル管理
  gTexMan.Init(&ctx.core->SRVMan());

  // Sprite は SpriteManager に委譲
  gSpriteMan.Init(gDevice, &gTexMan);

  // Model は ModelManager に委譲
  gModelMan.Init(gDevice, &gTexMan);

  // Sphere は SphereManager に委譲
  gSphereMan.Init(gDevice, &gTexMan);

  // Light は LightManager に委譲（default slot を作る）
  gLightMan.Init(gDevice);

  // PointLight は PointLightManager に委譲（default slot を作る）
  gPointLightMan.Init(gDevice);
  gSpotLightMan.Init(gDevice);
  gAreaLightMan.Init(gDevice);

  // CameraCB: Upload に置いて Map しっぱなしで更新する
  gCameraCB = CreateBufferResource(gDevice, sizeof(CameraCB));
  gCameraCB->Map(0, nullptr, reinterpret_cast<void **>(&gCameraCBMapped));

  // FogOverlayCB: 画面全体の白い霧（寒い表現）
  gFogCB = CreateBufferResource(gDevice, sizeof(FogOverlayCB));
  gFogCB->Map(0, nullptr, reinterpret_cast<void **>(&gFogCBMapped));
  if (gFogCBMapped) {
    // 既定値（後から DrawFogOverlay の引数で上書きされる）
    *gFogCBMapped = FogOverlayCB{};
  }

  // 初期化
  gView = MakeIdentity4x4();
  gProj = MakeIdentity4x4();

  gCL = nullptr;
  gCurrentBlendMode = BlendMode::kBlendModeNone;

  gInitialized = true;
}

void Term() {
  if (!gInitialized) {
    return;
  }

  gModelMan.Term();
  gSphereMan.Term();

  // Sprite は SpriteManager に委譲
  gSpriteMan.Term();

  // Light は LightManager に委譲
  gLightMan.Term();

  // PointLight は PointLightManager に委譲
  gPointLightMan.Term();
  gSpotLightMan.Term();
  gAreaLightMan.Term();

  gPrim2D.ptr.reset();
  gPrim2D.inUse = false;

  if (gCameraCB) {
    if (gCameraCBMapped) {
      gCameraCB->Unmap(0, nullptr);
      gCameraCBMapped = nullptr;
    }
    gCameraCB->Release();
    gCameraCB = nullptr;
  }

  if (gFogCB) {
    if (gFogCBMapped) {
      gFogCB->Unmap(0, nullptr);
      gFogCBMapped = nullptr;
    }
    gFogCB->Release();
    gFogCB = nullptr;
  }

  gTexMan.Term();

  gCtxRef = nullptr;
  gDevice = nullptr;
  gSrvHeap = nullptr;
  gCL = nullptr;

  gInitialized = false;
}

// ----------------------------------------------------------------------------
// Frame shared state
// ----------------------------------------------------------------------------

void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
               const RC::Vector3 camWorldPos) {
  // View / Proj は各オブジェクトの Update に渡す
  gView = view;
  gProj = proj;

  // カメラのワールド位置はシェーダー側で視線方向などに使える
  if (gCameraCBMapped) {
    gCameraCBMapped->worldPos = camWorldPos;
    gCameraCBMapped->_pad = 0.0f;
  }
}

// ----------------------------------------------------------------------------
// DirectionalLight
// ----------------------------------------------------------------------------

int CreateDirectionalLight(LightActivateMode activateMode) {
  const int h = gLightMan.Create();
  if (h < 0) {
    return h;
  }

  if (activateMode == LightActivateMode::Set) {
    gLightMan.SetActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    // まだ明示アクティブが無い時だけ採用
    if (!gLightMan.HasExplicitActive()) {
      gLightMan.SetActive(h);
    }
  }
  // None は何もしない

  return h;
}

void DestroyDirectionalLight(int lightHandle) {
  gLightMan.Destroy(lightHandle);
}

void SetActiveDirectionalLight(int lightHandle) {
  // -1: 明示的なアクティブ無し（描画時は default slot を使用）
  gLightMan.SetActive(lightHandle);
}

int GetActiveDirectionalLightHandle() {
  if (!gInitialized) {
    return -1;
  }

  const int explicitH = gLightMan.GetActiveHandle();
  if (explicitH >= 0) {
    return explicitH;
  }

  // 明示的なアクティブが無い場合は default(0) を返す
  return gLightMan.DefaultHandle();
}

DirectionalLightSource *GetDirectionalLightPtr(int lightHandle) {
  return gLightMan.Get(lightHandle);
}

void DrawImGuiDirectionalLight(int lightHandle, const char *name) {
  // -1 を渡されたら「実効アクティブ」で表示する（Set してなくても編集できる）
  if (lightHandle < 0) {
    lightHandle = GetActiveDirectionalLightHandle();
  }
  gLightMan.DrawImGui(lightHandle, name);
}

void SetDirectionalLightEnabled(int lightHandle, bool enabled) {
  if (auto *p = gLightMan.Get(lightHandle)) {
    p->SetEnabled(enabled);
    // 反映（DataForGPU を Sync する）
    (void)gLightMan.GetCBAddress(lightHandle);
  }
}

bool IsDirectionalLightEnabled(int lightHandle) {
  if (const auto *p = gLightMan.Get(lightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActiveDirectionalLightEnabled(bool enabled) {
  if (auto *p = gLightMan.GetActive()) {
    p->SetEnabled(enabled);
    // 反映
    (void)gLightMan.GetActiveCBAddress();
  }
}

bool IsActiveDirectionalLightEnabled() {
  if (const auto *p = gLightMan.GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

// ==============================
// PointLight
// ==============================

int CreatePointLight(LightActivateMode activateMode) {
  const int h = gPointLightMan.Create();
  if (h < 0) {
    return h;
  }

  // 生成した時点で「アクティブに渡す」オプション
  if (activateMode == LightActivateMode::Set) {
    gPointLightMan.ClearActive();
    (void)gPointLightMan.AddActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    // 追加に失敗（満杯）しても生成自体は成功しているので h は返す
    // ※必要ならここでログを出してもOK
    (void)gPointLightMan.AddActive(h);
  }

  return h;
}

void DestroyPointLight(int pointLightHandle) {
  gPointLightMan.Destroy(pointLightHandle);
}

void SetActivePointLight(int pointLightHandle) {
  // 互換: 旧 SetActive は「それ1個だけを使う」扱いにする
  gPointLightMan.ClearActive();
  if (pointLightHandle >= 0) {
    (void)gPointLightMan.AddActive(pointLightHandle);
  }
}

int GetActivePointLightHandle() {
  // 互換: 先頭のアクティブを返す（無ければ -1）
  if (gPointLightMan.GetActiveCount() <= 0) {
    return -1;
  }
  return gPointLightMan.GetActiveHandleAt(0);
}

void ClearActivePointLights() { gPointLightMan.ClearActive(); }

bool AddActivePointLight(int pointLightHandle) {
  return gPointLightMan.AddActive(pointLightHandle);
}

void RemoveActivePointLight(int pointLightHandle) {
  gPointLightMan.RemoveActive(pointLightHandle);
}

int GetActivePointLightCount() { return gPointLightMan.GetActiveCount(); }

int GetActivePointLightHandleAt(int index) {
  return gPointLightMan.GetActiveHandleAt(index);
}

PointLightSource *GetPointLightPtr(int pointLightHandle) {
  return gPointLightMan.Get(pointLightHandle);
}

void DrawImGuiPointLight(int pointLightHandle, const char *name) {
  gPointLightMan.DrawImGui(pointLightHandle, name);
}

void SetPointLightEnabled(int pointLightHandle, bool enabled) {
  if (auto *p = gPointLightMan.Get(pointLightHandle)) {
    p->SetEnabled(enabled);
  }
}

bool IsPointLightEnabled(int pointLightHandle) {
  if (const auto *p = gPointLightMan.Get(pointLightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActivePointLightEnabled(bool enabled) {
  if (auto *p = gPointLightMan.GetActive()) {
    p->SetEnabled(enabled);
  }
}

bool IsActivePointLightEnabled() {
  if (const auto *p = gPointLightMan.GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

// ==============================
// SpotLight
// ==============================

int CreateSpotLight(LightActivateMode activateMode) {
  const int h = gSpotLightMan.Create();
  if (h < 0) {
    return h;
  }

  if (activateMode == LightActivateMode::Set) {
    gSpotLightMan.ClearActive();
    (void)gSpotLightMan.AddActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    (void)gSpotLightMan.AddActive(h);
  }

  return h;
}

void DestroySpotLight(int spotLightHandle) {
  gSpotLightMan.Destroy(spotLightHandle);
}

void SetActiveSpotLight(int spotLightHandle) {
  gSpotLightMan.ClearActive();
  if (spotLightHandle >= 0) {
    (void)gSpotLightMan.AddActive(spotLightHandle);
  }
}

int GetActiveSpotLightHandle() {
  if (gSpotLightMan.GetActiveCount() <= 0) {
    return -1;
  }
  return gSpotLightMan.GetActiveHandleAt(0);
}

void ClearActiveSpotLights() { gSpotLightMan.ClearActive(); }

bool AddActiveSpotLight(int spotLightHandle) {
  return gSpotLightMan.AddActive(spotLightHandle);
}

void RemoveActiveSpotLight(int spotLightHandle) {
  gSpotLightMan.RemoveActive(spotLightHandle);
}

int GetActiveSpotLightCount() { return gSpotLightMan.GetActiveCount(); }

int GetActiveSpotLightHandleAt(int index) {
  return gSpotLightMan.GetActiveHandleAt(index);
}

SpotLightSource *GetSpotLightPtr(int spotLightHandle) {
  return gSpotLightMan.Get(spotLightHandle);
}

void DrawImGuiSpotLight(int spotLightHandle, const char *name) {
  gSpotLightMan.DrawImGui(spotLightHandle, name);
}

void SetSpotLightEnabled(int spotLightHandle, bool enabled) {
  if (auto *p = gSpotLightMan.Get(spotLightHandle)) {
    p->SetEnabled(enabled);
  }
}

bool IsSpotLightEnabled(int spotLightHandle) {
  if (const auto *p = gSpotLightMan.Get(spotLightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActiveSpotLightEnabled(bool enabled) {
  if (auto *p = gSpotLightMan.GetActive()) {
    p->SetEnabled(enabled);
  }
}

bool IsActiveSpotLightEnabled() {
  if (const auto *p = gSpotLightMan.GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

// ----------------------------------------------------------------------------
// Texture
// ----------------------------------------------------------------------------

int LoadTex(const std::string &path, bool srgb) {
  if (!gInitialized) {
    return -1;
  }

  // TextureManager 側で ID を採番して返す
  return gTexMan.LoadID(path, srgb);
}

D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle) {
  // 無効ハンドルは 0 を返しておく（呼び出し側で -1 を避けたい時用）
  if (!gInitialized || texHandle < 0) {
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }
  return gTexMan.GetSrv(texHandle);
}

// ----------------------------------------------------------------------------
// Initialization state
// ----------------------------------------------------------------------------

bool IsInitialized() { return gInitialized; }

ID3D12Device *GetDevice() { return gDevice; }
// ----------------------------------------------------------------------------
// 3D Pass
// ----------------------------------------------------------------------------

void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // 失敗時に "前フレームの gCL" が残ると危険なので、先に更新
  gCL = cl;
  gCtxRef = &ctx;

  // 3D のデフォルト
  // - object3d は BlendModeNone を基準にしている（必要なら SetBlendMode
  // で上書き）
  gCurrentBlendMode = kBlendModeNone;

  // まず object3d の PSO をセットしておく
  auto *pso = GetPipeline_(&ctx, "object3d", gCurrentBlendMode);
  if (!pso) {
    // PSO が無い = 描画できない
    gCL = nullptr;
    gCtxRef = nullptr;
    return;
  }

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  // NOTE:
  // Active light is applied at draw time (DrawModel/DrawSphere) so we can
  // switch lights per draw without copying into every object each frame.

  // バッチ描画を使うモデルは "どこまで描いたか" のカーソルを毎フレームリセット
  gModelMan.ResetAllBatchCursors();

  if (auto *prim = EnsurePrimitive3D_()) {
    prim->BeginFrame(gView, gProj, gCurrentBlendMode);
  }
}

// ----------------------------------------------------------------------------
// Model
// ----------------------------------------------------------------------------

int LoadModel(const std::string &path) {
  if (!gInitialized) {
    return -1;
  }

  return gModelMan.Load(path);
}

void DrawModel(int modelHandle, int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindPipeline_("object3d")) {
    return;
  }

  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  // Texture override (-1 => keep .mtl)
  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  // Apply active (or default) light at draw time.
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();

  // ★重要：ModelObject 側が b1 を外部ライトに差し替えられるようにする
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }

    m->Update(gView, gProj);
    m->Draw(gCL);
    return;
  }

  // Fallback: use model's own light CB
  m->SetExternalLightCBAddress(0);
  m->Update(gView, gProj);
  m->Draw(gCL);
}

void DrawModel(int modelHandle) { DrawModel(modelHandle, -1); }

void DrawModelNoCull(int modelHandle, int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindPipeline_("object3d_nocull")) {
    return;
  }

  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();

  // ★重要：ModelObject 側が b1 を外部ライトに差し替えられるようにする
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }

    m->Update(gView, gProj);
    m->Draw(gCL);
    return;
  }

  m->Update(gView, gProj);
  m->Draw(gCL);
}

void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindPipeline_("object3d_inst")) {
    return;
  }

  // 通常 Draw と同じように各種 CB をバインド
  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();

  // 重要：ModelObject 側が b1 を外部ライトに差し替えられるようにする
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }

    m->DrawBatch(gCL, gView, gProj, instances);
    return;
  }

  // Fallback
  m->SetExternalLightCBAddress(0);
  m->DrawBatch(gCL, gView, gProj, instances);
}

void DrawModelBatchColored(int modelHandle,
                           const std::vector<Transform> &instances,
                           const std::vector<Vector4> &colors,
                           int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  if (!BindPipeline_("object3d_inst")) {
    return;
  }

  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }

    m->DrawBatch(gCL, gView, gProj, instances, colors);
    return;
  }

  m->SetExternalLightCBAddress(0);
  m->DrawBatch(gCL, gView, gProj, instances, colors);
}

void DrawImGui3D(int modelHandle, const char *name) {
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  // LightManager が有効なら「共通ライトCB」が常に刺さるので
  // モデル側の Light UI（自前CB）は基本的に隠す。
  const bool showLightingUi = (gLightMan.GetActiveCBAddress() == 0);

  m->DrawImGui(name, showLightingUi);
}

void UnloadModel(int modelHandle) { gModelMan.Unload(modelHandle); }

Transform *GetModelTransformPtr(int modelHandle) {
  return gModelMan.GetTransformPtr(modelHandle);
}

void SetModelColor(int modelHandle, const Vector4 &color) {
  gModelMan.SetColor(modelHandle, color);
}

void SetModelLightingMode(int modelHandle, LightingMode m) {
  gModelMan.SetLightingMode(modelHandle, m);
}

void SetModelMesh(int modelHandle, const std::string &path) {
  gModelMan.SetMesh(modelHandle, path);
}

void ResetCursor(int modelHandle) { gModelMan.ResetCursor(modelHandle); }

// ===========================================
// Glass Model
// ===========================================

void DrawModelGlass(int modelHandle, int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = gCurrentBlendMode;
  gCurrentBlendMode = kBlendModePremultiplied;

  if (!BindPipeline_("object3d_glass")) {
    gCurrentBlendMode = saved;
    return;
  }

  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  // ※ここは好み：ガラスだけは shininess をライトから上書きしない方が自然
  // (shininess は本来 Material 側の値)
  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        // mat->shininess = active->GetShininess(); // ←ガラスでは外すのおすすめ
      }
    }
  }

  m->Update(gView, gProj);
  m->Draw(gCL);

  gCurrentBlendMode = saved;
}

void DrawModelGlassBatch(int modelHandle,
                         const std::vector<Transform> &instances,
                         int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = gCurrentBlendMode;
  gCurrentBlendMode = kBlendModePremultiplied;

  if (!BindPipeline_("object3d_glass_inst")) {
    gCurrentBlendMode = saved;
    return;
  }

  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  m->DrawBatch(gCL, gView, gProj, instances);

  gCurrentBlendMode = saved;
}

void DrawModelGlassTwoPass(int modelHandle, int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = gCurrentBlendMode;
  gCurrentBlendMode = kBlendModePremultiplied;

  // テクスチャ
  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  // Light
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);

  // ライティングモードだけ同期（shininess上書きはしない方が自然、君の方針でOK）
  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
      }
    }
  }

  // Updateは1回でOK
  m->Update(gView, gProj);

  auto bindCommon = [&]() {
    BindCameraCB_();
    BindPointLightCB_();
    BindSpotLightCB_();
    BindAreaLightCB_();
  };

  // 1) 背面（内側）を先に
  if (BindPipeline_("object3d_glass_front")) {
    bindCommon();
    m->Draw(gCL);
  }

  // 2) 表面（外側）を後に
  if (BindPipeline_("object3d_glass")) {
    bindCommon();
    m->Draw(gCL);
  }

  gCurrentBlendMode = saved;
}

void DrawModelGlassTwoPassBatch(int modelHandle,
                                const std::vector<Transform> &instances,
                                int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }

  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  const BlendMode saved = gCurrentBlendMode;
  gCurrentBlendMode = kBlendModePremultiplied;
  // テクスチャ
  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  // Light
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();
  m->SetExternalLightCBAddress(lightAddr);
  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
      }
    }
  }

  // Updateは1回でOK
  m->Update(gView, gProj);
  auto bindCommon = [&]() {
    BindCameraCB_();
    BindPointLightCB_();
    BindSpotLightCB_();
    BindAreaLightCB_();
  };

  // 1) 背面（内側）を先に
  if (BindPipeline_("object3d_glass_front_inst")) {
    bindCommon();
    m->DrawBatch(gCL, gView, gProj, instances);
  }

  // 2) 表面（外側）を後に
  if (BindPipeline_("object3d_glass_inst")) {
    bindCommon();
    m->DrawBatch(gCL, gView, gProj, instances);
  }

  gCurrentBlendMode = saved;
}

// ----------------------------------------------------------------------------
// 2D Pass
// ----------------------------------------------------------------------------

void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // 失敗時に "前フレームの gCL" が残ると危険なので、先に更新
  gCL = cl;
  gCtxRef = &ctx;

  // 2D のデフォルト
  // - sprite は普通のアルファブレンド前提なので Normal を基準にしている
  gCurrentBlendMode = kBlendModeNormal;

  // 3DのViewport/Scissorを壊す前に Primitive3D を吐く
  if (auto *prim = EnsurePrimitive3D_()) {
    if (prim->HasAny()) {
      const BlendMode saved = gCurrentBlendMode;
      gCurrentBlendMode = prim->BlendAt3D();

      if (BindPipeline_("primitive3d")) {
        gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        prim->Draw(gCL, true);
      }
      if (BindPipeline_("primitive3d_nodepth")) {
        gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        prim->Draw(gCL, false);
      }

      gCurrentBlendMode = saved;
    }
  }

  // 2D 描画は一時的に Viewport/Scissor を全画面にしておく
  // (Dx12Core 側の状態を壊したくないので「ローカル変数」を渡す)
  D3D12_VIEWPORT viewport{};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<float>(ctx.app->width);
  viewport.Height = static_cast<float>(ctx.app->height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  D3D12_RECT scissor{};
  scissor.left = 0;
  scissor.top = 0;
  scissor.right = static_cast<LONG>(ctx.app->width);
  scissor.bottom = static_cast<LONG>(ctx.app->height);

  cl->RSSetViewports(1, &viewport);
  cl->RSSetScissorRects(1, &scissor);

  // sprite の PSO をセットしておく
  auto *pso = GetPipeline_(&ctx, "sprite", gCurrentBlendMode);
  if (!pso) {
    gCL = nullptr;
    gCtxRef = nullptr;
    return;
  }

  // Primitive2D の BeginFrame
  // - ここで呼ぶことで、PreDraw2D の後の DrawLine/DrawBox/DrawCircle が使える
  if (auto *prim = EnsurePrimitive2D_()) {
    prim->BeginFrame();
  }

  cl->SetGraphicsRootSignature(pso->Root());
  cl->SetPipelineState(pso->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ----------------------------------------------------------------------------
// Sprite
// ----------------------------------------------------------------------------

int LoadSprite(const std::string &path, SceneContext &ctx, bool srgb) {
  if (!gInitialized) {
    return -1;
  }

  const float screenW = static_cast<float>(ctx.app->width);
  const float screenH = static_cast<float>(ctx.app->height);
  return gSpriteMan.Load(path, screenW, screenH, srgb);
}

void DrawSprite(int spriteHandle) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (!BindPipeline_("sprite")) {
    return;
  }

  gSpriteMan.Draw(spriteHandle, gCL);
}

void DrawSpriteRect(int spriteHandle, float srcX, float srcY, float srcW,
                    float srcH, float texW, float texH, float insetPx) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (!BindPipeline_("sprite")) {
    return;
  }

  gSpriteMan.DrawRect(spriteHandle, srcX, srcY, srcW, srcH, texW, texH, insetPx,
                      gCL);
}

void DrawSpriteRectUV(int spriteHandle, float u0, float v0, float u1,
                      float v1) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (!BindPipeline_("sprite")) {
    return;
  }

  gSpriteMan.DrawRectUV(spriteHandle, u0, v0, u1, v1, gCL);
}

void SetSpriteTransform(int spriteHandle, const Transform &t) {
  gSpriteMan.SetTransform(spriteHandle, t);
}

void SetSpriteColor(int spriteHandle, const Vector4 &color) {
  gSpriteMan.SetColor(spriteHandle, color);
}

void UnloadSprite(int spriteHandle) { gSpriteMan.Unload(spriteHandle); }

void SetSpriteScreenSize(int spriteHandle, float w, float h) {
  gSpriteMan.SetSize(spriteHandle, w, h);
}

void DrawImGui2D(int spriteHandle, const char *name) {
  gSpriteMan.DrawImGui(spriteHandle, name);
}

// ----------------------------------------------------------------------------
// Sphere
// ----------------------------------------------------------------------------

int GenerateSphereEx(int textureHandle, float radius, unsigned int sliceCount,
                     unsigned int stackCount, bool inward) {
  if (!gInitialized || !gDevice) {
    return -1;
  }

  return gSphereMan.Create(textureHandle, radius, sliceCount, stackCount,
                           inward);
}

int GenerateSphere(int textureHandle) {
  // 既定: radius=0.5, 16x16, inward=true
  return GenerateSphereEx(textureHandle);
}

void DrawSphere(int sphereHandle, int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }

  auto *s = gSphereMan.Get(sphereHandle);
  if (!s) {
    return;
  }

  // Sphere is drawn with the object3d pipeline.
  if (!BindPipeline_("object3d")) {
    return;
  }

  BindCameraCB_();
  BindPointLightCB_();
  BindSpotLightCB_();
  BindAreaLightCB_();

  gSphereMan.ApplyTexture(sphereHandle, texHandle);

  // Apply active (or default) light at draw time: bind the LightManager's CB
  // per draw.
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = gLightMan.GetActiveCBAddress();

  // ★重要：Sphere 側が b1 を外部ライトに差し替えられるようにする
  s->SetExternalLightCBAddress(lightAddr);
  if (lightAddr != 0) {
    if (const auto *active = gLightMan.GetActive()) {
      // Keep old behavior: inward spheres are usually unlit (sky dome).
      if (!s->GetInward()) {
        if (auto *mat = s->Mat()) {
          mat->lightingMode = active->GetLightingMode();
          mat->shininess = active->GetShininess();
        }
      }
    }
  }

  s->Update(gView, gProj);
  s->Draw(gCL);
}

void DrawSphereImGui(int sphereHandle, const char *name) {
  if (auto *s = gSphereMan.Get(sphereHandle)) {
    s->DrawImGui(name);
  }
}

void UnloadSphere(int sphereHandle) { gSphereMan.Unload(sphereHandle); }

Transform *GetSphereTransformPtr(int sphereHandle) {
  return gSphereMan.GetTransformPtr(sphereHandle);
}

void SetSphereColor(int sphereHandle, const Vector4 &color) {
  gSphereMan.SetColor(sphereHandle, color);
}

void SetSphereLightingMode(int sphereHandle, LightingMode m) {
  gSphereMan.SetLightingMode(sphereHandle, m);
}

// ----------------------------------------------------------------------------
// Primitive2D (Immediate)
// ----------------------------------------------------------------------------

// 注意（すごく大事）:
// Primitive2D が "1個の定数バッファ" を Map しっぱなしで使ってる実装の場合、
// DrawLine -> DrawBox -> DrawCircle みたいに連続で呼ぶと、
// 「全部の Draw が最後に書いた値で描かれる」現象が起きやすい。
// (CommandList は "GPU virtual address" を記録するだけで、その先のメモリ内容は
//  実行時に読まれるので、同じアドレスを何回も書き換えると最後の値になりがち)
// もし "最後に描いたやつしか出ない" 症状が出たら、Primitive2D 側で
// - CB をリングバッファ化して draw ごとにオフセットを変える
// - もしくは RootConstants（SetGraphicsRoot32BitConstants）にして CB を消す
// みたいに "draw ごとに値が固定される" 仕組みにすると安定する。

void DrawLine(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
              float thickness, float feather) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (!BindPipeline_("primitive2d")) {
    return;
  }

  auto *prim = EnsurePrimitive2D_();
  if (!prim) {
    return;
  }

  prim->SetLine(pos1, pos2, thickness, color, feather);
  prim->Draw(gCL);
}

void DrawBox(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
             kFillMode fillMode, float feather) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (!BindPipeline_("primitive2d")) {
    return;
  }

  auto *prim = EnsurePrimitive2D_();
  if (!prim) {
    return;
  }

  float thickness = 1.0f;

  prim->SetRect(pos1, pos2, fillMode, thickness, color, feather);
  prim->Draw(gCL);
}

void DrawCircle(const Vector2 &center, float radius, const Vector4 &color,
                kFillMode fillMode, float feather) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (!BindPipeline_("primitive2d")) {
    return;
  }

  auto *prim = EnsurePrimitive2D_();
  if (!prim) {
    return;
  }

  float thickness = 1.0f;

  prim->SetCircle(center, radius, fillMode, thickness, color, feather);
  prim->Draw(gCL);
}

void DrawTriangle(const Vector2 &pos1, const Vector2 &pos2, const Vector2 &pos3,
                  const Vector4 &color, kFillMode fillMode, float feather) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (!BindPipeline_("primitive2d")) {
    return;
  }

  auto *prim = EnsurePrimitive2D_();
  if (!prim) {
    return;
  }

  float thickness = 1.0f;

  prim->SetTriangle(pos1, pos2, pos3, fillMode, thickness, color, feather);
  prim->Draw(gCL);
}

void DrawLine3D(const Vector3 &a, const Vector3 &b, const Vector4 &color,
                bool depth) {
  if (!gInitialized || !gCL)
    return;

  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;

  prim->AddLine(a, b, color, depth);
}

void DrawAABB3D(const Vector3 &mn, const Vector3 &mx, const Vector4 &color,
                bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddAABB(mn, mx, color, depth);
}

void DrawGridXZ3D(int halfSize, float step, const Vector4 &color, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddGridXZ(halfSize, step, color, depth);
}

void DrawGridXY3D(int halfSize, float step, const Vector4 &color, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddGridXY(halfSize, step, color, depth);
}

void DrawGridYZ3D(int halfSize, float step, const Vector4 &color, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddGridYZ(halfSize, step, color, depth);
}

void DrawWireSphere3D(const Vector3 &center, float radius, const Vector4 &color,
                      int slices, int stacks, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddSphere(center, radius, color, slices, stacks, depth);
}

void DrawSphereRings3D(const Vector3 &center, float radius,
                       const Vector4 &color, int segments, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddSphereRings(center, radius, color, segments, depth);
}

void DrawArc3D(const Vector3 &center, const Vector3 &normal,
               const Vector3 &fromDir, float radius, float startRad,
               float endRad, const Vector4 &color, int segments, bool depth,
               bool drawToCenter) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddArc(center, normal, fromDir, radius, startRad, endRad, color,
               segments, depth, drawToCenter);
}

void DrawCapsule3D(const Vector3 &p0, const Vector3 &p1, float radius,
                   const Vector4 &color, int segments, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddCapsule(p0, p1, radius, color, segments, depth);
}

void DrawOBB3D(const Vector3 &center, const Vector3 &axisX,
               const Vector3 &axisY, const Vector3 &axisZ,
               const Vector3 &halfExtents, const Vector4 &color, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddOBB(center, axisX, axisY, axisZ, halfExtents, color, depth);
}

void DrawFrustumCorners3D(const Vector3 corners[8], const Vector4 &color,
                          bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddFrustum(corners, color, depth);
}

void DrawFrustum3D(const Vector3 &camPos, const Vector3 &forward,
                   const Vector3 &up, float fovYRad, float aspect, float nearZ,
                   float farZ, const Vector4 &color, bool depth) {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;
  prim->AddFrustumCamera(camPos, forward, up, fovYRad, aspect, nearZ, farZ,
                         color, depth);
}

// 2Dパスを呼ばないシーン用（任意）
void FlushPrimitive3D() {
  if (!gInitialized || !gCL)
    return;
  auto *prim = EnsurePrimitive3D_();
  if (!prim)
    return;

  if (!prim->HasAny()) {
    return;
  }

  const BlendMode saved = gCurrentBlendMode;
  gCurrentBlendMode = prim->BlendAt3D();

  if (BindPipeline_("primitive3d")) {
    gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    prim->Draw(gCL, true);
  }
  if (BindPipeline_("primitive3d_nodepth")) {
    gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    prim->Draw(gCL, false);
  }

  gCurrentBlendMode = saved;
}

// ----------------------------------------------------------------------------
// Fog overlay (cold mist)
// ----------------------------------------------------------------------------

void DrawFogOverlay(float timeSec, float intensity, float scale, float speed,
                    const Vector2 &wind, float feather, float bottomBias) {
  if (!gInitialized || !gCL || !gCtxRef || !gCtxRef->pipelineManager) {
    return;
  }

  // Fog パラメータ更新（Mapしっぱなし）
  if (gFogCBMapped) {
    gFogCBMapped->timeSec = timeSec;
    gFogCBMapped->intensity = intensity;
    gFogCBMapped->scale = scale;
    gFogCBMapped->speed = speed;
    gFogCBMapped->wind = wind;
    gFogCBMapped->feather = feather;
    gFogCBMapped->bottomBias = bottomBias;
  }

  // fog.normal を明示的に取得（gCurrentBlendMode には依存しない）
  auto *pso = gCtxRef->pipelineManager->Get(
      PipelineManager::MakeKey("fog", kBlendModeNormal));
  if (!pso) {
    return;
  }

  // 念のため viewport/scissor を画面に合わせる（PreDraw2D を呼んでるなら不要）
  if (gCtxRef->app) {
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(gCtxRef->app->width);
    viewport.Height = static_cast<float>(gCtxRef->app->height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    gCL->RSSetViewports(1, &viewport);

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(gCtxRef->app->width);
    scissor.bottom = static_cast<LONG>(gCtxRef->app->height);
    gCL->RSSetScissorRects(1, &scissor);
  }

  gCL->SetGraphicsRootSignature(pso->Root());
  gCL->SetPipelineState(pso->PSO());
  gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam[0] = b0 (PS)
  if (gFogCB) {
    gCL->SetGraphicsRootConstantBufferView(0, gFogCB->GetGPUVirtualAddress());
  }

  // フルスクリーントライアングル（VB/IB不要）
  gCL->DrawInstanced(3, 1, 0, 0);
}

void SetFogOverlayColor(const Vector4 &color) {
  if (!gInitialized) {
    return;
  }
  if (gFogCBMapped) {
    gFogCBMapped->color = color;
  }
}

// ----------------------------------------------------------------------------
// BlendMode
// ----------------------------------------------------------------------------

void SetBlendMode(BlendMode blendMode) {
  // これを変えると BindPipeline_ が引く PSO が変わる
  // - 例) PreDraw2D の直後に Add にして DrawSprite を呼ぶと sprite+Add を探す
  gCurrentBlendMode = blendMode;
}

BlendMode GetBlendMode() { return gCurrentBlendMode; }

// ============================================================================
// AreaLight（Rect） API
// ============================================================================

int CreateAreaLight(LightActivateMode activateMode) {
  if (!gInitialized) {
    return -1;
  }
  const int h = gAreaLightMan.Create();
  if (h < 0) {
    return -1;
  }

  if (activateMode == LightActivateMode::Set) {
    gAreaLightMan.ClearActive();
    gAreaLightMan.AddActive(h);
  } else if (activateMode == LightActivateMode::Add) {
    gAreaLightMan.AddActive(h);
  }
  // None の時は何もしない
  return h;
}

void DestroyAreaLight(int areaLightHandle) {
  if (!gInitialized) {
    return;
  }
  gAreaLightMan.Destroy(areaLightHandle);
}

void SetActiveAreaLight(int areaLightHandle) {
  if (!gInitialized) {
    return;
  }
  gAreaLightMan.SetActive(areaLightHandle);
}

int GetActiveAreaLightHandle() {
  if (!gInitialized) {
    return -1;
  }
  return gAreaLightMan.GetActiveHandle();
}

void ClearActiveAreaLights() {
  if (!gInitialized) {
    return;
  }
  gAreaLightMan.ClearActive();
}

bool AddActiveAreaLight(int areaLightHandle) {
  if (!gInitialized) {
    return false;
  }
  return gAreaLightMan.AddActive(areaLightHandle);
}

void RemoveActiveAreaLight(int areaLightHandle) {
  if (!gInitialized) {
    return;
  }
  gAreaLightMan.RemoveActive(areaLightHandle);
}

int GetActiveAreaLightCount() {
  if (!gInitialized) {
    return 0;
  }
  return gAreaLightMan.GetActiveCount();
}

int GetActiveAreaLightHandleAt(int index) {
  if (!gInitialized) {
    return -1;
  }
  return gAreaLightMan.GetActiveHandleAt(index);
}

AreaLightSource *GetAreaLightPtr(int areaLightHandle) {
  if (!gInitialized) {
    return nullptr;
  }
  return gAreaLightMan.Get(areaLightHandle);
}

void DrawImGuiAreaLight(int areaLightHandle, const char *name) {
  if (!gInitialized) {
    return;
  }
  gAreaLightMan.DrawImGui(areaLightHandle, name);
}

void SetAreaLightEnabled(int areaLightHandle, bool enabled) {
  if (!gInitialized)
    return;
  if (auto *p = gAreaLightMan.Get(areaLightHandle)) {
    p->SetEnabled(enabled);
  }
}

bool IsAreaLightEnabled(int areaLightHandle) {
  if (!gInitialized)
    return false;
  if (const auto *p = gAreaLightMan.Get(areaLightHandle)) {
    return p->IsEnabled();
  }
  return false;
}

void SetActiveAreaLightEnabled(bool enabled) {
  if (!gInitialized)
    return;
  if (auto *p = gAreaLightMan.GetActive()) {
    p->SetEnabled(enabled);
  }
}

bool IsActiveAreaLightEnabled() {
  if (!gInitialized)
    return false;
  if (const auto *p = gAreaLightMan.GetActive()) {
    return p->IsEnabled();
  }
  return false;
}

} // namespace RC
