#include "PrimitiveObject.h"
#include "Dx12/Dx12Core.h"
#include "Graphics/Mesh/MeshGenerator.h"
#include <Math/Math.h>

namespace RC {

std::unique_ptr<PrimitiveObject> PrimitiveObject::CreatePlane(SceneContext &ctx, float w, float h) {
  auto obj = std::make_unique<PrimitiveObject>();
  obj->Initialize(ctx, MeshGenerator::GeneratePlane(w, h));
  return obj;
}

std::unique_ptr<PrimitiveObject> PrimitiveObject::CreateBox(SceneContext &ctx, float w, float h, float d) {
  auto obj = std::make_unique<PrimitiveObject>();
  obj->Initialize(ctx, MeshGenerator::GenerateBox(w, h, d));
  return obj;
}

std::unique_ptr<PrimitiveObject> PrimitiveObject::CreateSphere(SceneContext &ctx, float r, int sl, int st) {
  auto obj = std::make_unique<PrimitiveObject>();
  obj->Initialize(ctx, MeshGenerator::GenerateSphere(r, sl, st));
  return obj;
}

std::unique_ptr<PrimitiveObject> PrimitiveObject::CreateCylinder(SceneContext &ctx, float r, float h, int s) {
  auto obj = std::make_unique<PrimitiveObject>();
  obj->Initialize(ctx, MeshGenerator::GenerateCylinder(r, h, s));
  return obj;
}

std::unique_ptr<PrimitiveObject> PrimitiveObject::CreateCapsule(SceneContext &ctx, float r, float h) {
  auto obj = std::make_unique<PrimitiveObject>();
  obj->Initialize(ctx, MeshGenerator::GenerateCapsule(r, h));
  return obj;
}

std::unique_ptr<PrimitiveObject> PrimitiveObject::CreateTorus(SceneContext &ctx, float mjR, float mnR) {
  auto obj = std::make_unique<PrimitiveObject>();
  obj->Initialize(ctx, MeshGenerator::GenerateTorus(mjR, mnR));
  return obj;
}

void PrimitiveObject::Initialize(SceneContext &ctx, const ModelData &data) {
  mesh_.Initialize(ctx.core->GetDevice(), data);
}

void PrimitiveObject::Update() {
  // 基本的にはTransform の更新は描画時に行われる
}

void PrimitiveObject::Render(ID3D12GraphicsCommandList *cl) {
  mesh_.Draw(cl, MakeAffineMatrix(mesh_.T().scale, mesh_.T().rotation, mesh_.T().translation));
}

} // namespace RC
