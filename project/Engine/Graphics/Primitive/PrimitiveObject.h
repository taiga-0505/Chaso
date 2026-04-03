#pragma once
#include "Graphics/Mesh/PrimitiveMesh.h"
#include "Scene.h"
#include <memory>

namespace RC {

/// <summary>
/// プログラムから動的に生成可能なプリミティブオブジェクト
/// - MeshGenerator の出力を内部でラップ
/// - モデル(FBX)を介さずパラメータ指定で即時作成可能
/// </summary>
class PrimitiveObject {
public:
  // 各種形状のファクトリメソッド
  static std::unique_ptr<PrimitiveObject> CreatePlane(SceneContext &ctx, float w=1.0f, float h=1.0f);
  static std::unique_ptr<PrimitiveObject> CreateBox(SceneContext &ctx, float w=1.0f, float h=1.0f, float d=1.0f);
  static std::unique_ptr<PrimitiveObject> CreateSphere(SceneContext &ctx, float r=1.0f, int sl=32, int st=16);
  static std::unique_ptr<PrimitiveObject> CreateCylinder(SceneContext &ctx, float r=0.5f, float h=1.0f, int s=32);
  static std::unique_ptr<PrimitiveObject> CreateCapsule(SceneContext &ctx, float r=0.5f, float h=2.0f);
  static std::unique_ptr<PrimitiveObject> CreateTorus(SceneContext &ctx, float mjR=1.0f, float mnR=0.2f);

  void Initialize(SceneContext &ctx, const ModelData &data);
  void Update();
  void Render(ID3D12GraphicsCommandList *cl);

  Transform &T() { return mesh_.T(); }
  Material *Mat() { return mesh_.Mat(); }
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) { mesh_.SetTexture(srv); }
  void SetVisible(bool v) { mesh_.SetVisible(v); }

private:
  PrimitiveMesh mesh_;
};

} // namespace RC
