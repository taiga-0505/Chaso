#include "ModelRender.h"
#include "RenderCommonInternal.h"

// ここは「描画のために必要な実体」だけ include する。
// - ModelObject / Sphere は Draw/Update/Mat 等を呼ぶため、完全型が必要。
#include "Light/Directional/DirectionalLightManager.h"
#include "Model3D/ModelManager.h"
#include "Model3d/ModelObject.h"
#include "Sphere/Sphere.h"
#include "Sphere/SphereManager.h"
#include "Texture/TextureManager/TextureManager.h"

namespace RC {

namespace {

// このモジュールが「Model/Sphere 管理」を持つ（RenderCommon は持たない）
RC::ModelManager gModelMan;
RC::SphereManager gSphereMan;

// RC::Init / RC::Term / RC::PreDraw3D から呼ばれる拡張フック
void OnInitModelModule_(SceneContext & /*ctx*/) {
  ID3D12Device *dev = detail::GetDevice();
  if (!dev) {
    return;
  }
  auto &tm = detail::GetTexMan();

  gModelMan.Init(dev, &tm);
  gSphereMan.Init(dev, &tm);
}

void OnTermModelModule_() {
  gModelMan.Term();
  gSphereMan.Term();
}

void OnPreDraw3DModelModule_() {
  // バッチ描画用カーソルを毎フレームリセット
  gModelMan.ResetAllBatchCursors();
}

// 自動登録（この cpp がリンクされれば勝手に登録される）
struct AutoRegisterHooks_ {
  AutoRegisterHooks_() {
    detail::RegisterInitHook(&OnInitModelModule_);
    detail::RegisterTermHook(&OnTermModelModule_);
    detail::RegisterPreDraw3DHook(&OnPreDraw3DModelModule_);
  }
};
static AutoRegisterHooks_ gAutoRegisterHooks_;

} // namespace

// ============================================================================
// Public API (今まで通り RC::xxx で呼べるやつ)
// ============================================================================

// ----------------------------------------------------------------------------
// Model
// ----------------------------------------------------------------------------

int LoadModel(const std::string &path) {
  if (!detail::IsInitialized()) {
    return -1;
  }
  return gModelMan.Load(path);
}

void DrawModel(int modelHandle, int texHandle) {
  ModelRender::DrawModel(modelHandle, texHandle);
}

void DrawModel(int modelHandle) {
  ModelRender::DrawModel(modelHandle, -1);
}

void DrawModelNoCull(int modelHandle, int texHandle) {
  ModelRender::DrawModelNoCull(modelHandle, texHandle);
}

void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle) {
  ModelRender::DrawModelBatch(modelHandle, instances, texHandle);
}

void DrawImGui3D(int modelHandle, const char *name) {
  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  // LightManager が有効なら「共通ライトCB」が常に刺さるので
  // モデル側の Light UI（自前CB）は基本的に隠す。
  const bool showLightingUi = (detail::GetDirLightMan().GetActiveCBAddress() == 0);
  m->DrawImGui(name, showLightingUi);
}

void UnloadModel(int modelHandle) {
  gModelMan.Unload(modelHandle);
}

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

void ResetCursor(int modelHandle) {
  gModelMan.ResetCursor(modelHandle);
}

// ----------------------------------------------------------------------------
// Sphere
// ----------------------------------------------------------------------------

int GenerateSphereEx(int textureHandle, float radius, unsigned int sliceCount,
                     unsigned int stackCount, bool inward) {
  if (!detail::IsInitialized() || !detail::GetDevice()) {
    return -1;
  }
  return gSphereMan.Create(textureHandle, radius, sliceCount, stackCount, inward);
}

int GenerateSphere(int textureHandle) {
  // 既定: radius=0.5, 16x16, inward=true
  return GenerateSphereEx(textureHandle);
}

void DrawSphere(int sphereHandle, int texHandle) {
  ModelRender::DrawSphere(sphereHandle, texHandle);
}

void DrawSphereImGui(int sphereHandle, const char *name) {
  if (auto *s = gSphereMan.Get(sphereHandle)) {
    s->DrawImGui(name);
  }
}

void UnloadSphere(int sphereHandle) {
  gSphereMan.Unload(sphereHandle);
}

Transform *GetSphereTransformPtr(int sphereHandle) {
  return gSphereMan.GetTransformPtr(sphereHandle);
}

void SetSphereColor(int sphereHandle, const Vector4 &color) {
  gSphereMan.SetColor(sphereHandle, color);
}

void SetSphereLightingMode(int sphereHandle, LightingMode m) {
  gSphereMan.SetLightingMode(sphereHandle, m);
}

// ============================================================================
// Draw implementation (ModelRender)
// ============================================================================

// ----------------------------------------------------------------------------
// Model
// ----------------------------------------------------------------------------

void ModelRender::DrawModel(int modelHandle, int texHandle) {
  if (!detail::IsInitialized()) {
    return;
  }
  ID3D12GraphicsCommandList *cl = detail::GetCL();
  if (!cl) {
    return;
  }

  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  // Texture override (-1 => keep .mtl)
  if (texHandle >= 0) {
    m->SetTexture(detail::GetTexMan().GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  // Apply active (or default) light at draw time.
  auto &dirLightMan = detail::GetDirLightMan();
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = dirLightMan.GetActiveCBAddress();

  // ★重要：ModelObject 側が b1 を外部ライトに差し替えられるようにする
  m->SetExternalLightCBAddress(lightAddr);

  // Active light があるなら、そのパラメータに合わせて Material を更新
  if (lightAddr != 0) {
    if (const auto *active = dirLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }

    m->Update(detail::GetView(), detail::GetProj());
    m->Draw(cl);
    return;
  }

  // Fallback: use model's own light CB
  m->SetExternalLightCBAddress(0);
  m->Update(detail::GetView(), detail::GetProj());
  m->Draw(cl);
}

void ModelRender::DrawModelNoCull(int modelHandle, int texHandle) {
  if (!detail::IsInitialized()) {
    return;
  }
  ID3D12GraphicsCommandList *cl = detail::GetCL();
  if (!cl) {
    return;
  }

  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  if (!detail::BindPipeline("object3d_nocull")) {
    return;
  }

  detail::BindCameraCB();
  detail::BindPointLightCB();
  detail::BindSpotLightCB();
  detail::BindAreaLightCB();

  if (texHandle >= 0) {
    m->SetTexture(detail::GetTexMan().GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  auto &dirLightMan = detail::GetDirLightMan();
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = dirLightMan.GetActiveCBAddress();

  // ★重要：ModelObject 側が b1 を外部ライトに差し替えられるようにする
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = dirLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }

    m->Update(detail::GetView(), detail::GetProj());
    m->Draw(cl);
    return;
  }

  m->Update(detail::GetView(), detail::GetProj());
  m->Draw(cl);
}

void ModelRender::DrawModelBatch(int modelHandle,
                                const std::vector<Transform> &instances,
                                int texHandle) {
  if (!detail::IsInitialized()) {
    return;
  }
  ID3D12GraphicsCommandList *cl = detail::GetCL();
  if (!cl) {
    return;
  }

  auto *m = gModelMan.Get(modelHandle);
  if (!m) {
    return;
  }

  if (!detail::BindPipeline("object3d_inst")) {
    return;
  }

  // 通常 Draw と同じように各種 CB をバインド
  detail::BindCameraCB();
  detail::BindPointLightCB();
  detail::BindSpotLightCB();
  detail::BindAreaLightCB();

  if (texHandle >= 0) {
    m->SetTexture(detail::GetTexMan().GetSrv(texHandle));
  } else {
    m->ResetTextureToMtl();
  }

  auto &dirLightMan = detail::GetDirLightMan();
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr = dirLightMan.GetActiveCBAddress();

  // 重要：ModelObject 側が b1 を外部ライトに差し替えられるようにする
  m->SetExternalLightCBAddress(lightAddr);

  if (lightAddr != 0) {
    if (const auto *active = dirLightMan.GetActive()) {
      if (Material *mat = m->Mat()) {
        mat->lightingMode = active->GetLightingMode();
        mat->shininess = active->GetShininess();
      }
    }

    m->DrawBatch(cl, detail::GetView(), detail::GetProj(), instances);
    return;
  }

  // Fallback
  m->SetExternalLightCBAddress(0);
  m->DrawBatch(cl, detail::GetView(), detail::GetProj(), instances);
}

// ----------------------------------------------------------------------------
// Sphere
// ----------------------------------------------------------------------------

void ModelRender::DrawSphere(int sphereHandle, int texHandle) {
  if (!detail::IsInitialized()) {
    return;
  }
  ID3D12GraphicsCommandList *cl = detail::GetCL();
  if (!cl) {
    return;
  }

  auto *s = gSphereMan.Get(sphereHandle);
  if (!s) {
    return;
  }

  // Sphere is drawn with the object3d pipeline.
  if (!detail::BindPipeline("object3d")) {
    return;
  }

  detail::BindCameraCB();
  detail::BindPointLightCB();
  detail::BindSpotLightCB();
  detail::BindAreaLightCB();

  gSphereMan.ApplyTexture(sphereHandle, texHandle);

  // Apply active (or default) light at draw time: bind the LightManager's CB per draw.
  auto &dirLightMan = detail::GetDirLightMan();
  D3D12_GPU_VIRTUAL_ADDRESS lightAddr = dirLightMan.GetActiveCBAddress();

  // ★重要：Sphere 側が b1 を外部ライトに差し替えられるようにする
  s->SetExternalLightCBAddress(lightAddr);
  if (lightAddr != 0) {
    if (const auto *active = dirLightMan.GetActive()) {
      // Keep old behavior: inward spheres are usually unlit (sky dome).
      if (!s->GetInward()) {
        if (auto *mat = s->Mat()) {
          mat->lightingMode = active->GetLightingMode();
          mat->shininess = active->GetShininess();
        }
      }
    }
  }

  s->Update(detail::GetView(), detail::GetProj());
  s->Draw(cl);
}

} // namespace RC
