#pragma once
#include "Graphics/Mesh/PrimitiveMesh.h"
#include "Scene.h"
#include <memory>

namespace RC {

/// @class PrimitiveObject
/// @brief プログラムから動的に生成可能なプリミティブ図形オブジェクト
/// @details MeshGenerator の出力を内部でラップし、外部ファイル(FBX等)を介さずパラメータ指定で即時作成・表示が可能です。
class PrimitiveObject {
public:
  // 各種形状のファクトリメソッド

  /// @brief 平面(Plane)を生成する
  /// @param ctx シーンコンテキスト
  /// @param w 幅
  /// @param h 高さ
  /// @return 生成された PrimitiveObject の unique_ptr
  static std::unique_ptr<PrimitiveObject> CreatePlane(SceneContext &ctx, float w=1.0f, float h=1.0f);

  /// @brief 立方体(Box)を生成する
  /// @param ctx シーンコンテキスト
  /// @param w 幅
  /// @param h 高さ
  /// @param d 奥行き
  /// @return 生成された PrimitiveObject の unique_ptr
  static std::unique_ptr<PrimitiveObject> CreateBox(SceneContext &ctx, float w=1.0f, float h=1.0f, float d=1.0f);

  /// @brief 球(Sphere)を生成する
  /// @param ctx シーンコンテキスト
  /// @param r 半径
  /// @param sl 経度分割数(Slices)
  /// @param st 緯度分割数(Stacks)
  /// @return 生成された PrimitiveObject の unique_ptr
  static std::unique_ptr<PrimitiveObject> CreateSphere(SceneContext &ctx, float r=1.0f, int sl=32, int st=16);

  /// @brief 円柱(Cylinder)を生成する
  /// @param ctx シーンコンテキスト
  /// @param r 半径
  /// @param h 高さ
  /// @param s 側面分割数
  /// @return 生成された PrimitiveObject の unique_ptr
  static std::unique_ptr<PrimitiveObject> CreateCylinder(SceneContext &ctx, float r=0.5f, float h=1.0f, int s=32);

  /// @brief カプセル(Capsule)を生成する
  /// @param ctx シーンコンテキスト
  /// @param r 半径
  /// @param h 高さ
  /// @return 生成された PrimitiveObject の unique_ptr
  static std::unique_ptr<PrimitiveObject> CreateCapsule(SceneContext &ctx, float r=0.5f, float h=2.0f);

  /// @brief トーラス(Torus)を生成する
  /// @param ctx シーンコンテキスト
  /// @param mjR 主半径(中心からリングの中心まで)
  /// @param mnR 副半径(リング自体の太さの半分)
  /// @return 生成された PrimitiveObject の unique_ptr
  static std::unique_ptr<PrimitiveObject> CreateTorus(SceneContext &ctx, float mjR=1.0f, float mnR=0.2f);

  /// @brief オブジェクトを初期化する
  /// @param ctx シーンコンテキスト
  /// @param data 頂点・インデックスを含むモデルデータ
  void Initialize(SceneContext &ctx, const ModelData &data);

  /// @brief 更新処理
  void Update();

  /// @brief 描画処理
  /// @param cl グラフィックスコマンドリスト
  void Render(ID3D12GraphicsCommandList *cl);

  /// @brief トランスフォーム情報を取得する（読み書き可能）
  /// @return Transform への参照
  Transform &T() { return mesh_.T(); }

  /// @brief マテリアル情報を取得する（読み書き可能）
  /// @return Material ポインタ
  Material *Mat() { return mesh_.Mat(); }

  /// @brief テクスチャを設定する
  /// @param srv テクスチャの GPU ハンドル
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) { mesh_.SetTexture(srv); }

  /// @brief オブジェクトの可視状態を設定する
  /// @param v true で表示
  void SetVisible(bool v) { mesh_.SetVisible(v); }

private:
  PrimitiveMesh mesh_; ///< 描画に使用する内部プリミティブメッシュ
};

} // namespace RC
