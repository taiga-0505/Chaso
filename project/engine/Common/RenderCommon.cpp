#include "RenderCommon.h"

#include "Dx12/Dx12Core.h"
#include "Light/Light.h"
#include "PipelineManager.h"
#include "Primitive2D/Primitive2D.h"
#include "Sprite2D/Sprite2D.h"
#include "Texture/TextureManager/TextureManager.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>

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

struct SpriteSlot {
  std::unique_ptr<Sprite2D> ptr;
  bool inUse = false;
  int texHandle = -1; // 便利用: どのテクスチャから作ったか
};

struct SphereSlot {
  std::unique_ptr<Sphere> ptr;
  bool inUse = false;
  int defaultTexHandle = -1; // 生成時に渡されたテクスチャ（DrawSphere の省略時に使う）
};

struct LightSlot {
  RC::Light light;
  bool inUse = false;
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
std::vector<SpriteSlot> gSprites;
std::vector<LightSlot> gLights;

ID3D12Device *gDevice = nullptr;
DescriptorHeap *gSrvHeap = nullptr; // いまは直接使ってない（互換用に保持）
TextureManager gTexMan;

Matrix4x4 gView;
Matrix4x4 gProj;

ID3D12GraphicsCommandList *gCL = nullptr; // Draw* が使う "今フレ" の CommandList
SceneContext *gCtxRef = nullptr;          // PipelineManager にアクセスするために保持

static ID3D12Resource *gCameraCB = nullptr;
static CameraCB *gCameraCBMapped = nullptr;

bool gInitialized = false;
static BlendMode gCurrentBlendMode = BlendMode::kBlendModeNone;

// 現在アクティブなライトハンドル（-1 = 無し）
int gActiveLightHandle = -1;

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
  return (h >= 0 && h < static_cast<int>(gSpheres.size()) && gSpheres[h].inUse &&
          gSpheres[h].ptr);
}

int AllocLightSlot_() {
  for (int i = 0; i < static_cast<int>(gLights.size()); ++i) {
    if (!gLights[i].inUse) {
      return i;
    }
  }
  gLights.emplace_back();
  return static_cast<int>(gLights.size()) - 1;
}

bool IsValidLight_(int h) {
  return (h >= 0 && h < static_cast<int>(gLights.size()) && gLights[h].inUse);
}

// ============================================================================
// Pipeline 選択
// ----------------------------------------------------------------------------
// prefix + BlendMode で PipelineManager から引いてくる。
// 見つからない場合は normal にフォールバック。
// - 例) prefix="sprite", mode=Add の PSO が無い時は sprite+Normal を使う
// ============================================================================
static GraphicsPipeline *GetPipeline_(SceneContext *ctx, std::string_view prefix,
                                      BlendMode mode) {
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

  // CameraCB: Upload に置いて Map しっぱなしで更新する
  gCameraCB = CreateBufferResource(gDevice, sizeof(CameraCB));
  gCameraCB->Map(0, nullptr, reinterpret_cast<void **>(&gCameraCBMapped));

  // 初期化
  gModels.clear();
  gSpheres.clear();
  gSprites.clear();
  gLights.clear();

  gActiveLightHandle = -1;
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
  gSprites.clear();
  gSpheres.clear();
  gLights.clear();

  gActiveLightHandle = -1;

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
  if (!gInitialized) {
    return -1;
  }

  const int h = AllocLightSlot_();
  gLights[h].inUse = true;
  gLights[h].light = Light{}; // 既定値で初期化

  // まだアクティブが無いなら、最初のライトを自動でアクティブにする
  if (gActiveLightHandle < 0) {
    gActiveLightHandle = h;
  }

  return h;
}

void DestroyLight(int lightHandle) {
  if (!IsValidLight_(lightHandle)) {
    return;
  }

  gLights[lightHandle].inUse = false;

  // アクティブが消えたら無効化（必要なら外側で別ライトを SetActiveLight してね）
  if (gActiveLightHandle == lightHandle) {
    gActiveLightHandle = -1;
  }
}

void SetActiveLight(int lightHandle) {
  if (!IsValidLight_(lightHandle)) {
    return;
  }
  gActiveLightHandle = lightHandle;
}

int GetActiveLightHandle() { return gActiveLightHandle; }

Light *GetLightPtr(int lightHandle) {
  if (!IsValidLight_(lightHandle)) {
    return nullptr;
  }
  return &gLights[lightHandle].light;
}

void DrawImGuiLight(int lightHandle, const char *name) {
  if (!IsValidLight_(lightHandle)) {
    return;
  }
  gLights[lightHandle].light.DrawImGui(name);
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
  // - object3d は BlendModeNone を基準にしている（必要なら SetBlendMode で上書き）
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

  // --------------------------------------------------------------------------
  // アクティブライトの反映
  // --------------------------------------------------------------------------
  // RenderCommon 側で "今のライト" を決めて、各オブジェクトにコピーする。
  // - モデル単体にライトを持たせたい場合は、ここをやめて各モデル側に任せる
  // --------------------------------------------------------------------------
  if (gActiveLightHandle >= 0 && IsValidLight_(gActiveLightHandle)) {
    const Light &srcLight = gLights[gActiveLightHandle].light;
    const DirectionalLight &srcDir = srcLight.Data();
    const int mode = srcLight.GetLightingMode();
    const float shininess = srcLight.GetShininess();

    // Model
    for (auto &slot : gModels) {
      if (!slot.inUse || !slot.ptr) {
        continue;
      }

      if (auto *dst = slot.ptr->Light()) {
        *dst = srcDir;
      }
      if (auto *mat = slot.ptr->Mat()) {
        mat->lightingMode = mode;
        mat->shininess = shininess;
      }
    }

    // Sphere（外向きメッシュだけ）
    for (auto &slot : gSpheres) {
      if (!slot.inUse || !slot.ptr) {
        continue;
      }
      if (slot.ptr->GetInward()) {
        continue; // 天球（内向き）にはライトを当てない想定
      }

      if (auto *dst = slot.ptr->Light()) {
        *dst = srcDir;
      }
      if (auto *mat = slot.ptr->Mat()) {
        mat->lightingMode = mode;
        mat->shininess = shininess;
      }
    }
  }

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

  // 注意:
  // - Draw のたびに PSO をセットしている。
  //   (DrawSphere/DrawSprite/Primitive が途中で別PSOに変えても安全にするため)
  if (!BindPipeline_("object3d")) {
    return;
  }

  BindCameraCB_();

  auto &m = gModels[modelHandle].ptr;

  // 明示テクスチャが指定されていれば差し替え（-1 なら .mtl のまま）
  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  // カメラ反映 → 描画
  m->Update(gView, gProj);
  m->Draw(gCL);
}

void DrawModel(int modelHandle) { DrawModel(modelHandle, -1); }

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

  // instances はワールド変換の配列（Model 側が StructuredBuffer などで描く想定）
  m->DrawBatch(gCL, gView, gProj, instances);
}

void DrawImGui3D(int modelHandle, const char *name) {
  if (!IsValidModel_(modelHandle)) {
    return;
  }

  auto &m = gModels[modelHandle].ptr;

  // Light がアクティブならモデル側の Lighting UI は隠す
  const bool showLightingUi =
      !(gActiveLightHandle >= 0 && IsValidLight_(gActiveLightHandle));

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

  // テクスチャロード
  const int texHandle = gTexMan.LoadID(path, srgb);
  if (texHandle < 0) {
    return -1;
  }

  // スプライトは「現状」スロット再利用してない（必要なら Alloc 関数を作る）
  const int handle = static_cast<int>(gSprites.size());
  gSprites.emplace_back();

  auto &s = gSprites[handle];
  s.ptr = std::make_unique<Sprite2D>();

  // 初期化
  s.ptr->Initialize(gDevice, static_cast<float>(ctx.app->width),
                    static_cast<float>(ctx.app->height));
  s.ptr->SetTexture(gTexMan.GetSrv(texHandle));

  // 初期値（必要なら外側で SetSpriteTransform / SetSpriteScreenSize 等で上書き）
  s.ptr->SetSize(100, 100);
  s.ptr->T().translation = {0, 0, 0};
  s.ptr->SetVisible(true);

  s.texHandle = texHandle;
  s.inUse = true;

  return handle;
}

void DrawSprite(int spriteHandle) {
  if (!gInitialized || !gCL) {
    return;
  }

  if (spriteHandle < 0 || spriteHandle >= static_cast<int>(gSprites.size())) {
    return;
  }

  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr) {
    return;
  }

  if (!BindPipeline_("sprite")) {
    return;
  }

  s.ptr->Update();
  s.ptr->Draw(gCL);
}

void SetSpriteTransform(int spriteHandle, const Transform &t) {
  if (spriteHandle < 0 || spriteHandle >= static_cast<int>(gSprites.size())) {
    return;
  }
  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr) {
    return;
  }
  s.ptr->T() = t;
}

void SetSpriteColor(int spriteHandle, const Vector4 &color) {
  if (spriteHandle < 0 || spriteHandle >= static_cast<int>(gSprites.size())) {
    return;
  }
  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr) {
    return;
  }
  s.ptr->SetColor(color);
}

void UnloadSprite(int spriteHandle) {
  if (spriteHandle < 0 || spriteHandle >= static_cast<int>(gSprites.size())) {
    return;
  }
  auto &s = gSprites[spriteHandle];
  if (!s.inUse) {
    return;
  }
  s.ptr.reset();
  s.inUse = false;
}

void SetSpriteScreenSize(int spriteHandle, float w, float h) {
  if (spriteHandle < 0 || spriteHandle >= static_cast<int>(gSprites.size())) {
    return;
  }
  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr) {
    return;
  }
  s.ptr->SetSize(w, h);
}

void DrawImGui2D(int spriteHandle, const char *name) {
  if (spriteHandle < 0 || spriteHandle >= static_cast<int>(gSprites.size())) {
    return;
  }
  auto &m = gSprites[spriteHandle].ptr;
  if (!m) {
    return;
  }
  m->DrawImGui(name);
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

  // Sphere は object3d パイプラインで描く
  if (!BindPipeline_("object3d")) {
    return;
  }

  BindCameraCB_();

  auto &s = gSpheres[sphereHandle].ptr;

  // テクスチャ差し替え（明示指定 > 生成時指定）
  const int useTex = (texHandle >= 0) ? texHandle
                                      : gSpheres[sphereHandle].defaultTexHandle;

  if (useTex >= 0) {
    s->SetTexture(gTexMan.GetSrv(useTex));
  }

  // NOTE:
  // useTex < 0 の場合、Sphere 側が SRV を参照する RootParam が無効になる可能性がある。
  // "天球 = 必ず有効テクスチャ" 運用にしておくのが安全。

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

void DrawLine(const Vector2 &p0, const Vector2 &p1, float thickness,
              const Vector4 &color, float feather) {
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

  prim->SetLine(p0, p1, thickness, color, feather);
  prim->Draw(gCL);
}

void DrawBox(const Vector2 &mn, const Vector2 &mx, bool stroke,
             float thickness, const Vector4 &color, float feather) {
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

  prim->SetRect(mn, mx, stroke, thickness, color, feather);
  prim->Draw(gCL);
}

void DrawCircle(const Vector2 &center, float radius, bool stroke,
                float thickness, const Vector4 &color, float feather) {
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

  prim->SetCircle(center, radius, stroke, thickness, color, feather);
  prim->Draw(gCL);
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
