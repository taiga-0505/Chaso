#include "RenderCommon.h"
#include "Dx12/Dx12Core.h"
#include "Light/Light.h"
#include "PipelineManager.h"
#include "Sprite2D/Sprite2D.h"
#include "Texture/TextureManager/TextureManager.h"
#include <algorithm>

namespace RC {

namespace {
struct ModelSlot {
  std::unique_ptr<Model3D> ptr;
  bool inUse = false;
};

struct SpriteSlot {
  std::unique_ptr<Sprite2D> ptr;
  bool inUse = false;
  int texHandle = -1;
};

struct SphereSlot {
  std::unique_ptr<Sphere> ptr;
  bool inUse = false;
  int defaultTexHandle = -1; // 生成時に渡されたテクスチャを保持
};

struct LightSlot {
  RC::Light light;
  bool inUse = false;
};

std::vector<ModelSlot> gModels;   // モデルスロット
std::vector<SphereSlot> gSpheres; // スフィアスロット
std::vector<SpriteSlot> gSprites; // スプライトスロット
std::vector<LightSlot> gLights;   // ライトスロット

// --- グローバル状態（アプリ全体で共有） ---
ID3D12Device *gDevice = nullptr;
DescriptorHeap *gSrvHeap = nullptr;
TextureManager gTexMan;                   // IDベースで管理
Matrix4x4 gView, gProj;                   // 今フレのカメラ
ID3D12GraphicsCommandList *gCL = nullptr; // 現在のコマンドリスト
SceneContext *gCtxRef = nullptr;
bool gInitialized = false;
static BlendMode gCurrentBlendMode = BlendMode::kBlendModeNormal;

// 現在アクティブなライトハンドル
int gActiveLightHandle = -1;

// 空きスロット探す（なければ拡張）
int AllocModelSlot_() {
  for (int i = 0; i < (int)gModels.size(); ++i) {
    if (!gModels[i].inUse)
      return i;
  }
  gModels.emplace_back();
  return (int)gModels.size() - 1;
}

bool IsValidModel_(int h) {
  return (h >= 0 && h < (int)gModels.size() && gModels[h].inUse &&
          gModels[h].ptr);
}

int AllocSphereSlot_() {
  for (int i = 0; i < (int)gSpheres.size(); ++i) {
    if (!gSpheres[i].inUse)
      return i;
  }
  gSpheres.emplace_back();
  return (int)gSpheres.size() - 1;
}

bool IsValidSphere_(int h) {
  return (h >= 0 && h < (int)gSpheres.size() && gSpheres[h].inUse &&
          gSpheres[h].ptr);
}

int AllocLightSlot_() {
  for (int i = 0; i < (int)gLights.size(); ++i) {
    if (!gLights[i].inUse)
      return i;
  }
  gLights.emplace_back();
  return (int)gLights.size() - 1;
}

bool IsValidLight_(int h) {
  return (h >= 0 && h < (int)gLights.size() && gLights[h].inUse);
}

} // namespace

// ====== 起動時一回 ======
void Init(SceneContext &ctx) {
  if (gInitialized)
    return;
  gCtxRef = &ctx;
  gDevice = ctx.core->GetDevice();
  gSrvHeap = &ctx.core->SRV();

  gTexMan.Init(gDevice, gSrvHeap);

  gModels.clear();
  gSpheres.clear();
  gLights.clear();
  gActiveLightHandle = -1;
  gView = MakeIdentity4x4();
  gProj = MakeIdentity4x4();
  gCL = nullptr;

  gInitialized = true;
}

void Term() {
  if (!gInitialized)
    return;
  gModels.clear();
  gSprites.clear();
  gSpheres.clear();
  gLights.clear();
  gActiveLightHandle = -1;
  gTexMan.Term();
  gCtxRef = nullptr;
  gDevice = nullptr;
  gSrvHeap = nullptr;
  gCL = nullptr;
  gInitialized = false;
}

// ====== フレーム共有 ======
void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj) {
  gView = view;
  gProj = proj;
}

// ====== Light 管理 ======

int CreateLight() {
  if (!gInitialized)
    return -1;

  const int h = AllocLightSlot_();
  gLights[h].inUse = true;
  gLights[h].light = Light{}; // 既定値で初期化

  // 最初に作られたライトを自動でアクティブにする
  if (gActiveLightHandle < 0) {
    gActiveLightHandle = h;
  }

  return h;
}

void DestroyLight(int lightHandle) {
  if (!IsValidLight_(lightHandle))
    return;

  gLights[lightHandle].inUse = false;

  if (gActiveLightHandle == lightHandle) {
    gActiveLightHandle = -1;
  }
}

void SetActiveLight(int lightHandle) {
  if (!IsValidLight_(lightHandle))
    return;
  gActiveLightHandle = lightHandle;
}

int GetActiveLightHandle() { return gActiveLightHandle; }

Light *GetLightPtr(int lightHandle) {
  if (!IsValidLight_(lightHandle))
    return nullptr;
  return &gLights[lightHandle].light;
}

void DrawImGuiLight(int lightHandle, const char *name) {
  if (!IsValidLight_(lightHandle))
    return;
  gLights[lightHandle].light.DrawImGui(name);
}

// ====== ロード ======
int LoadModel(const std::string &path) {
  if (!gInitialized)
    return -1;
  const int handle = AllocModelSlot_();

  auto m = std::make_unique<Model3D>();
  m->Initialize(gDevice); // 戻り値はチェックしない

  m->SetTextureManager(&gTexMan);
  if (!m->LoadObj(path)) {
    gModels[handle].inUse = false;
    return -1;
  }

  gModels[handle].ptr = std::move(m);
  gModels[handle].inUse = true;
  return handle;
}

int LoadTex(const std::string &path, bool srgb) {
  if (!gInitialized)
    return -1;
  return gTexMan.LoadID(path, srgb);
}

D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle) {
  if (!gInitialized || texHandle < 0) {
    return D3D12_GPU_DESCRIPTOR_HANDLE{0};
  }
  return gTexMan.GetSrv(texHandle);
}

// ====== 描画セットアップ ======
void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // 3D オブジェクト用 PSO/RtSig/トポロジ
  cl->SetGraphicsRootSignature(ctx.objectPSO->Root());
  cl->SetPipelineState(ctx.objectPSO->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  gCL = cl; // DrawModel から使う

  // アクティブライトを全 Model3D に流し込む
  if (gActiveLightHandle >= 0 && IsValidLight_(gActiveLightHandle)) {
    const Light &srcLight = gLights[gActiveLightHandle].light;
    const DirectionalLight &srcDir = srcLight.Data();
    const int mode = srcLight.GetLightingMode();

    for (auto &slot : gModels) {
      if (!slot.inUse || !slot.ptr)
        continue;

      // ライト方向/色/強さ
      if (auto *dst = slot.ptr->Light()) { // DirectionalLight*
        *dst = srcDir;
      }

      // マテリアル側の lightingMode も上書き
      if (auto *mat = slot.ptr->Mat()) { // Material*
        mat->lightingMode = mode;
      }
    }
  }

  // 各モデルのバッチカーソルをリセット
  for (auto &slot : gModels) {
    if (slot.inUse && slot.ptr)
      slot.ptr->ResetBatchCursor();
  }
}

// ====== 描画 ======
void DrawModel(int modelHandle, int texHandle) {
  if (!gInitialized || !gCL)
    return;
  if (!IsValidModel_(modelHandle))
    return;

  auto &m = gModels[modelHandle].ptr;

  if (gCtxRef && gCtxRef->pipelineManager) {
    GraphicsPipeline *pso =
        gCtxRef->pipelineManager->GetModelPipeline(gCurrentBlendMode);
    gCL->SetGraphicsRootSignature(pso->Root());
    gCL->SetPipelineState(pso->PSO());
    gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }

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
  if (!gInitialized || !gCL)
    return;
  if (!IsValidModel_(modelHandle))
    return;
  if (instances.empty())
    return;

  auto &m = gModels[modelHandle].ptr;

  if (gCtxRef && gCtxRef->pipelineManager) {
    GraphicsPipeline *pso =
        gCtxRef->pipelineManager->GetModelPipeline(gCurrentBlendMode);
    gCL->SetGraphicsRootSignature(pso->Root());
    gCL->SetPipelineState(pso->PSO());
    gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }

  if (texHandle >= 0) {
    m->SetTexture(gTexMan.GetSrv(texHandle));
  }

  m->DrawBatch(gCL, gView, gProj, instances);
}

void DrawImGui3D(int modelHandle, const char *name) {
  if (!IsValidModel_(modelHandle))
    return;
  auto &m = gModels[modelHandle].ptr;

  // Light がアクティブならモデル側の Lighting UI は隠す
  bool showLightingUi =
      !(gActiveLightHandle >= 0 && IsValidLight_(gActiveLightHandle));

  m->DrawImGui(name, showLightingUi);
}

// ====== 解放 ======
void UnloadModel(int modelHandle) {
  if (!IsValidModel_(modelHandle))
    return;
  gModels[modelHandle].ptr.reset();
  gModels[modelHandle].inUse = false;
}

Transform *GetModelTransformPtr(int modelHandle) {
  if (!IsValidModel_(modelHandle))
    return nullptr;
  return &gModels[modelHandle].ptr->T();
}

void SetModelColor(int modelHandle, const Vector4 &color) {
  if (!IsValidModel_(modelHandle))
    return;
  gModels[modelHandle].ptr->SetColor(color);
}

void SetModelLightingMode(int modelHandle, LightingMode m) {
  if (!IsValidModel_(modelHandle))
    return;
  gModels[modelHandle].ptr->SetLightingMode(m);
}

void ResetCursor(int modelHandle) {
  if (!IsValidModel_(modelHandle))
    return;
  gModels[modelHandle].ptr->ResetBatchCursor();
}

void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  // 2D描画用に、一時的にビューポートとシザーを全画面に設定する
  // (Dx12Core に保存されている設定は変更しない)

  // 1. 全画面のビューポートを「ローカル変数」として作成
  D3D12_VIEWPORT viewport{};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<float>(ctx.app->width);
  viewport.Height = static_cast<float>(ctx.app->height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  // 2. 全画面のシザー矩形を「ローカル変数」として作成
  D3D12_RECT scissor{};
  scissor.left = 0;
  scissor.top = 0;
  scissor.right = static_cast<LONG>(ctx.app->width);
  scissor.bottom = static_cast<LONG>(ctx.app->height);

  // 3. コマンドリストに「直接」セットする
  cl->RSSetViewports(1, &viewport);
  cl->RSSetScissorRects(1, &scissor);

  cl->SetGraphicsRootSignature(ctx.spritePSO->Root());
  cl->SetPipelineState(ctx.spritePSO->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  gCL = cl;
  gCtxRef = &ctx;
}

int LoadSprite(const std::string &path, SceneContext &ctx, bool srgb) {
  if (!gInitialized)
    return -1;

  // テクスチャロード
  int texHandle = gTexMan.LoadID(path, srgb);
  if (texHandle < 0)
    return -1;

  // スプライトスロット確保
  int handle = (int)gSprites.size();
  gSprites.emplace_back();
  auto &s = gSprites[handle];
  s.ptr = std::make_unique<Sprite2D>();

  // 初期化
  s.ptr->Initialize(gDevice, (float)ctx.app->width, (float)ctx.app->height);
  s.ptr->SetTexture(gTexMan.GetSrv(texHandle));
  s.ptr->SetSize(100, 100);
  s.ptr->T().translation = {0, 0, 0};
  s.ptr->SetVisible(true);
  s.texHandle = texHandle;
  s.inUse = true;

  return handle;
}

void DrawSprite(int spriteHandle) {
  if (!gInitialized || !gCL)
    return;
  if (spriteHandle < 0 || spriteHandle >= (int)gSprites.size())
    return;

  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr)
    return;

  if (gCtxRef && gCtxRef->pipelineManager) {
    GraphicsPipeline *pso =
        gCtxRef->pipelineManager->GetSpritePipeline(gCurrentBlendMode);
    gCL->SetGraphicsRootSignature(pso->Root());
    gCL->SetPipelineState(pso->PSO());
    gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }

  s.ptr->Update();
  s.ptr->Draw(gCL);
}

void SetSpriteTransform(int spriteHandle, const Transform &t) {
  if (spriteHandle < 0 || spriteHandle >= (int)gSprites.size())
    return;
  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr)
    return;
  s.ptr->T() = t;
}

void SetSpriteColor(int spriteHandle, const Vector4 &color) {
  if (spriteHandle < 0 || spriteHandle >= (int)gSprites.size())
    return;
  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr)
    return;
  s.ptr->SetColor(color);
}

void UnloadSprite(int spriteHandle) {
  if (spriteHandle < 0 || spriteHandle >= (int)gSprites.size())
    return;
  auto &s = gSprites[spriteHandle];
  if (!s.inUse)
    return;
  s.ptr.reset();
  s.inUse = false;
}

void SetSpriteScreenSize(int spriteHandle, float w, float h) {
  if (spriteHandle < 0 || spriteHandle >= (int)gSprites.size())
    return;
  auto &s = gSprites[spriteHandle];
  if (!s.inUse || !s.ptr)
    return;
  s.ptr->SetSize(w, h);
}

void DrawImGui2D(int spriteHandle, const char *name) {
  if (spriteHandle < 0 || spriteHandle >= (int)gSprites.size())
    return;
  auto &m = gSprites[spriteHandle].ptr;
  m->DrawImGui(name);
}

// ====== Sphere 生成 ======
int GenerateSphereEx(int textureHandle, float radius, unsigned int sliceCount,
                     unsigned int stackCount, bool inward) {
  if (!gInitialized || !gDevice)
    return -1;

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
  // 既定の 0.5f, 16x16, inward=true
  return GenerateSphereEx(textureHandle);
}

// ====== Sphere 描画 ======
void DrawSphere(int sphereHandle, int texHandle) {
  if (!gInitialized || !gCL)
    return;
  if (!IsValidSphere_(sphereHandle))
    return;

  auto &s = gSpheres[sphereHandle].ptr;

  // ブレンドモードに応じた PSO を適用（モデルと同じパイプラインを流用）
  if (gCtxRef && gCtxRef->pipelineManager) {
    GraphicsPipeline *pso =
        gCtxRef->pipelineManager->GetModelPipeline(gCurrentBlendMode);
    gCL->SetGraphicsRootSignature(pso->Root());
    gCL->SetPipelineState(pso->PSO());
    gCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }

  // テクスチャ差し替え（明示指定 > 生成時指定）
  int useTex =
      (texHandle >= 0) ? texHandle : gSpheres[sphereHandle].defaultTexHandle;
  if (useTex >= 0) {
    s->SetTexture(gTexMan.GetSrv(useTex));
  }
  // ※ useTex < 0 の場合、RootParam2 に無効 SRV を渡すことになるので、
  //   必ず有効なテクスチャをどちらかで設定してください。

  // カメラ反映 → 描画
  s->Update(gView, gProj);
  s->Draw(gCL);
}

void DrawSphereImGui(int sphereHandle, const char *name) {
  if (!IsValidSphere_(sphereHandle))
    return;
  auto &s = gSpheres[sphereHandle].ptr;
  s->DrawImGui(name);
}

// ====== Sphere 後始末 ======
void UnloadSphere(int sphereHandle) {
  if (!IsValidSphere_(sphereHandle))
    return;
  gSpheres[sphereHandle].ptr.reset();
  gSpheres[sphereHandle].inUse = false;
  gSpheres[sphereHandle].defaultTexHandle = -1;
}

// ====== Sphere 付随ユーティリティ ======
Transform *GetSphereTransformPtr(int sphereHandle) {
  if (!IsValidSphere_(sphereHandle))
    return nullptr;
  return &gSpheres[sphereHandle].ptr->T();
}

void SetSphereColor(int sphereHandle, const Vector4 &color) {
  if (!IsValidSphere_(sphereHandle))
    return;
  if (auto *mat = gSpheres[sphereHandle].ptr->Mat()) {
    mat->color = color;
  }
}

void SetSphereLightingMode(int sphereHandle, LightingMode m) {
  if (!IsValidSphere_(sphereHandle))
    return;
  if (auto *mat = gSpheres[sphereHandle].ptr->Mat()) {
    mat->lightingMode = (int)m; // Material のメンバは int
  }
}

void RC::SetBlendMode(BlendMode blendMode) { gCurrentBlendMode = blendMode; }

BlendMode GetBlendMode() { return gCurrentBlendMode; }

} // namespace RC
