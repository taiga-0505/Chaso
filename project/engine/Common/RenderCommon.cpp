#include "RenderCommon.h"
#include "Dx12/Dx12Core.h"
#include "Model3D/Model3D.h"
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

std::vector<SpriteSlot> gSprites;

// --- グローバル状態（アプリ全体で共有） ---
ID3D12Device *gDevice = nullptr;
DescriptorHeap *gSrvHeap = nullptr;
TextureManager gTexMan;                   // IDベースで管理
std::vector<ModelSlot> gModels;           // モデルスロット
Matrix4x4 gView, gProj;                   // 今フレのカメラ
ID3D12GraphicsCommandList *gCL = nullptr; // 現在のコマンドリスト
SceneContext *gCtxRef = nullptr;
bool gInitialized = false;
static BlendMode gCurrentBlendMode = BlendMode::kBlendModeNormal;

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
  m->DrawImGui(name);
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

void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  cl->SetGraphicsRootSignature(ctx.spritePSO->Root());
  cl->SetPipelineState(ctx.spritePSO->PSO());
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  gCL = cl;
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
  s.ptr->SetScreenSize(w, h);
}

void DrawImGui2D(int spriteHandle, const char *name) {
  if (spriteHandle < 0 || spriteHandle >= (int)gSprites.size())
    return;
  auto &m = gSprites[spriteHandle].ptr;
  m->DrawImGui(name);
}

void RC::SetBlendMode(BlendMode blendMode) {
  gCurrentBlendMode = blendMode;
}

BlendMode GetBlendMode() { return gCurrentBlendMode; }

} // namespace RenderCommon
