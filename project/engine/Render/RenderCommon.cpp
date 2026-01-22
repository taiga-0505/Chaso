#include "RenderCommon.h"

#include "Dx12/Dx12Core.h"
#include "Light/LightManager.h"
#include "PipelineManager.h"
#include "Primitive2D/Primitive2D.h"
#include "Sprite2D/Sprite2D.h"
#include "Sprite2D/SpriteManager.h"
#include "Texture/TextureManager/TextureManager.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>
#include "struct.h"

namespace RC {

// ============================================================================
// このファイルの目的
// ----------------------------------------------------------------------------
// RenderCommon は「Scene から呼べる描画ファサード」。
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
// 内部スロット（ハンドル管理）
// ----------------------------------------------------------------------------
// 「ハンドル = vector の index」
// - inUse=false のスロットは再利用される
// - ptr が nullptr のハンドルは無効
// ============================================================================

struct ModelSlot {
  std::unique_ptr<Model3D> ptr;
  bool inUse = false;
};


struct SphereSlot {
  std::unique_ptr<Sphere> ptr;
  bool inUse = false;
  int defaultTexHandle =
      -1; // 生成時に渡されたテクスチャ（DrawSphere の省略時に使う）
};



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

struct PrimitiveSlot {
  std::unique_ptr<Primitive2D> ptr;
  bool inUse = false;
};

// ============================================================================
// Mesh cache
// ----------------------------------------------------------------------------
// 同じ .obj を何回 LoadModel しても、Mesh は共有したい。
// - key はパスを正規化して作る
// - weak_ptr なので、どこにも参照が無くなると自動で破棄される
// ============================================================================
static std::unordered_map<std::string, std::weak_ptr<ModelMesh>> gMeshCache;

static std::string NormalizeMeshKey_(const std::string &path) {
  namespace fs = std::filesystem;

  std::error_code ec;
  fs::path p(path);

  // 絶対パス化できるなら絶対パス。失敗したら相対のまま lexically_normal。
  fs::path abs = fs::absolute(p, ec);
  fs::path key = ec ? p.lexically_normal() : abs.lexically_normal();
  return key.string();
}

static std::shared_ptr<ModelMesh> GetOrLoadMesh_(ID3D12Device *device,
                                                 const std::string &path) {
  const std::string key = NormalizeMeshKey_(path);

  // すでにロード済みなら共有
  if (auto it = gMeshCache.find(key); it != gMeshCache.end()) {
    if (auto sp = it->second.lock()) {
      return sp;
    }
  }

  // 未ロードならロード
  auto mesh = std::make_shared<ModelMesh>();
  if (!mesh->LoadObj(device, path)) {
    return nullptr;
  }

  gMeshCache[key] = mesh;
  return mesh;
}

// ============================================================================
// グローバル状態（RenderCommon の "中の人"）
// ----------------------------------------------------------------------------
// - gCtxRef / gCL は PreDraw3D/PreDraw2D で毎フレーム更新
// - gView / gProj / gCameraCB は SetCamera で毎フレーム更新
// ============================================================================
std::vector<ModelSlot> gModels;
std::vector<SphereSlot> gSpheres;
RC::SpriteManager gSpriteMan;
RC::LightManager gLightMan;

ID3D12Device *gDevice = nullptr;
DescriptorHeap *gSrvHeap = nullptr; // いまは直接使ってない（互換用に保持）
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
// ハンドル/スロット関連ヘルパ
// ============================================================================

int AllocModelSlot_() {
  for (int i = 0; i < static_cast<int>(gModels.size()); ++i) {
    if (!gModels[i].inUse) {
      return i;
    }
  }
  gModels.emplace_back();
  return static_cast<int>(gModels.size()) - 1;
}

bool IsValidModel_(int h) {
  return (h >= 0 && h < static_cast<int>(gModels.size()) && gModels[h].inUse &&
          gModels[h].ptr);
}

int AllocSphereSlot_() {
  for (int i = 0; i < static_cast<int>(gSpheres.size()); ++i) {
    if (!gSpheres[i].inUse) {
      return i;
    }
  }
  gSpheres.emplace_back();
  return static_cast<int>(gSpheres.size()) - 1;
}

bool IsValidSphere_(int h) {
  return (h >= 0 && h < static_cast<int>(gSpheres.size()) &&
          gSpheres[h].inUse && gSpheres[h].ptr);
}



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

  // Light は LightManager に委譲（default slot を作る）
  gLightMan.Init(gDevice);

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
  gModels.clear();
  gSpheres.clear();
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

  gModels.clear();
  gMeshCache.clear();

  // Sprite は SpriteManager に委譲
  gSpriteMan.Term();

  // Light は LightManager に委譲
  gLightMan.Term();
  gSpheres.clear();

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
// Light
// ----------------------------------------------------------------------------

int CreateLight() {
  return gLightMan.Create();
}

void DestroyLight(int lightHandle) {
  gLightMan.Destroy(lightHandle);
}

void SetActiveLight(int lightHandle) {
  // -1: 明示的なアクティブ無し（描画時は default slot を使用）
  gLightMan.SetActive(lightHandle);
}

int GetActiveLightHandle() { return gLightMan.GetActiveHandle(); }

Light *GetLightPtr(int lightHandle) {
  return gLightMan.Get(lightHandle);
}

void DrawImGuiLight(int lightHandle, const char *name) {
  gLightMan.DrawImGui(lightHandle, name);
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

  // NOTE:
  // Active light is applied at draw time (DrawModel/DrawSphere) so we can
  // switch lights per draw without copying into every object each frame.

  // バッチ描画を使うモデルは "どこまで描いたか" のカーソルを毎フレームリセット
  for (auto &slot : gModels) {
    if (slot.inUse && slot.ptr) {
      slot.ptr->ResetBatchCursor();
    }
  }
}

// ----------------------------------------------------------------------------
// Model
// ----------------------------------------------------------------------------

int LoadModel(const std::string &path) {
  if (!gInitialized) {
    return -1;
  }

  const int handle = AllocModelSlot_();

  // ModelObject は Model3D の派生（想定）
  auto obj = std::make_unique<ModelObject>();
  obj->Initialize(gDevice);
  obj->SetTextureManager(&gTexMan);

  // Mesh はキャッシュを通す
  auto mesh = GetOrLoadMesh_(gDevice, path);
  if (!mesh) {
    gModels[handle].inUse = false;
    gModels[handle].ptr.reset();
    return -1;
  }

  obj->SetMesh(mesh);

  gModels[handle].ptr = std::move(obj);
  gModels[handle].inUse = true;
  return handle;
}

void DrawModel(int modelHandle, int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  if (!IsValidModel_(modelHandle)) {
    return;
  }

  if (!BindPipeline_("object3d")) {
    return;
  }

  BindCameraCB_();

  auto &m = gModels[modelHandle].ptr;

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
  if (!IsValidModel_(modelHandle)) {
    return;
  }

  if (!BindPipeline_("object3d_nocull")) {
    return;
  }

  BindCameraCB_();

  auto &m = gModels[modelHandle].ptr;

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
  if (!IsValidModel_(modelHandle)) {
    return;
  }
  if (instances.empty()) {
    return;
  }

  if (!BindPipeline_("object3d")) {
    return;
  }

  BindCameraCB_();

  auto &m = gModels[modelHandle].ptr;

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

    m->DrawBatch(gCL, gView, gProj, instances);
    return;
  }

  // Fallback
  m->SetExternalLightCBAddress(0);
  m->DrawBatch(gCL, gView, gProj, instances);
}


void DrawImGui3D(int modelHandle, const char *name) {
  if (!IsValidModel_(modelHandle)) {
    return;
  }

  auto &m = gModels[modelHandle].ptr;

  // LightManager が有効なら「共通ライトCB」が常に刺さるので
  // モデル側の Light UI（自前CB）は基本的に隠す。
  const bool showLightingUi = (gLightMan.GetActiveCBAddress() == 0);

  m->DrawImGui(name, showLightingUi);
}

void UnloadModel(int modelHandle) {
  if (!IsValidModel_(modelHandle)) {
    return;
  }
  gModels[modelHandle].ptr.reset();
  gModels[modelHandle].inUse = false;
}

Transform *GetModelTransformPtr(int modelHandle) {
  if (!IsValidModel_(modelHandle)) {
    return nullptr;
  }
  return &gModels[modelHandle].ptr->T();
}

void SetModelColor(int modelHandle, const Vector4 &color) {
  if (!IsValidModel_(modelHandle)) {
    return;
  }
  gModels[modelHandle].ptr->SetColor(color);
}

void SetModelLightingMode(int modelHandle, LightingMode m) {
  if (!IsValidModel_(modelHandle)) {
    return;
  }
  gModels[modelHandle].ptr->SetLightingMode(m);
}

void SetModelMesh(int modelHandle, const std::string &path) {
  if (!IsValidModel_(modelHandle)) {
    return;
  }

  auto mesh = GetOrLoadMesh_(gDevice, path);
  if (!mesh) {
    return;
  }

  gModels[modelHandle].ptr->SetMesh(mesh);

  // Mesh を変えると mtl のテクスチャ参照も変わる可能性があるので戻す
  gModels[modelHandle].ptr->ResetTextureToMtl();
}

void ResetCursor(int modelHandle) {
  if (!IsValidModel_(modelHandle)) {
    return;
  }
  gModels[modelHandle].ptr->ResetBatchCursor();
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

void UnloadSprite(int spriteHandle) {
  gSpriteMan.Unload(spriteHandle);
}

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

  const int handle = AllocSphereSlot_();

  auto s = std::make_unique<Sphere>();
  s->Initialize(gDevice, radius, sliceCount, stackCount, inward);

  // テクスチャ指定があれば設定
  if (textureHandle >= 0) {
    s->SetTexture(gTexMan.GetSrv(textureHandle));
  }

  gSpheres[handle].ptr = std::move(s);
  gSpheres[handle].inUse = true;
  gSpheres[handle].defaultTexHandle = textureHandle;

  return handle;
}

int GenerateSphere(int textureHandle) {
  // 既定: radius=0.5, 16x16, inward=true
  return GenerateSphereEx(textureHandle);
}

void DrawSphere(int sphereHandle, int texHandle) {
  if (!gInitialized || !gCL) {
    return;
  }
  if (!IsValidSphere_(sphereHandle)) {
    return;
  }

  // Sphere is drawn with the object3d pipeline.
  if (!BindPipeline_("object3d")) {
    return;
  }

  BindCameraCB_();

  auto &s = gSpheres[sphereHandle].ptr;

  // Texture override (explicit > default)
  const int useTex =
      (texHandle >= 0) ? texHandle : gSpheres[sphereHandle].defaultTexHandle;
  if (useTex >= 0) {
    s->SetTexture(gTexMan.GetSrv(useTex));
  }

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
  if (!IsValidSphere_(sphereHandle)) {
    return;
  }

  auto &s = gSpheres[sphereHandle].ptr;
  s->DrawImGui(name);
}

void UnloadSphere(int sphereHandle) {
  if (!IsValidSphere_(sphereHandle)) {
    return;
  }

  gSpheres[sphereHandle].ptr.reset();
  gSpheres[sphereHandle].inUse = false;
  gSpheres[sphereHandle].defaultTexHandle = -1;
}

Transform *GetSphereTransformPtr(int sphereHandle) {
  if (!IsValidSphere_(sphereHandle)) {
    return nullptr;
  }
  return &gSpheres[sphereHandle].ptr->T();
}

void SetSphereColor(int sphereHandle, const Vector4 &color) {
  if (!IsValidSphere_(sphereHandle)) {
    return;
  }

  if (auto *mat = gSpheres[sphereHandle].ptr->Mat()) {
    mat->color = color;
  }
}

void SetSphereLightingMode(int sphereHandle, LightingMode m) {
  if (!IsValidSphere_(sphereHandle)) {
    return;
  }

  if (auto *mat = gSpheres[sphereHandle].ptr->Mat()) {
    mat->lightingMode = static_cast<int>(m); // Material のメンバは int
  }
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
//
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

} // namespace RC
