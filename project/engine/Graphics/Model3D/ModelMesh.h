#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "function/function.h"
#include "struct.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <d3d12.h>
#include <filesystem>
#include <string>
#include <vector>

// ============================================================
// ModelMesh
// - assimpで .obj / .gltf / .glb を読み込む
// - 全Meshを1つのVertexBufferに連結
// - Node階層(Transform)を解析して DrawItem に展開
//   -> ModelObject側で DrawItem をループして描画できる
// ============================================================
class ModelMesh {
public:
  // Node階層を反映して描画するための「描画単位」
  struct DrawItem {
    uint32_t vertexStart = 0;     // VB上の開始頂点
    uint32_t vertexCount = 0;     // 頂点数
    uint32_t materialIndex = 0;   // scene->mMaterials の index
    uint32_t meshIndex = 0;       // scene->mMeshes の index（デバッグ用）
    RC::Matrix4x4 nodeWorld = {}; // Model空間での累積行列（Root→このNode）
    std::string nodeName;         // デバッグ用
  };

  ModelMesh() = default;
  ~ModelMesh();

  // modelPath: .obj / .gltf / .glb 直指定 or ディレクトリ指定（中から探す）
  bool LoadModel(ID3D12Device *device, const std::string &modelPath);

  // 互換：既存呼び出しは LoadObj のままでもOK（中でLoadModelを呼ぶ）
  bool LoadObj(ID3D12Device *device, const std::string &path) {
    return LoadModel(device, path);
  }

  // UV未設定(=0,0)に対し簡易的な球面UVを焼き込む
  void EnsureSphericalUVIfMissing();

  bool Ready() const { return vb_.resource != nullptr && vb_.vertexCount > 0; }
  const D3D12_VERTEX_BUFFER_VIEW &VBV() const { return vb_.view; }
  uint32_t VertexCount() const { return vb_.vertexCount; }

  // 互換用（昔は1枚だけだった）
  const MaterialData &MaterialFile() const { return materialFile_; }

  // 新：Material一覧 / Node / DrawItem
  const std::vector<MaterialData> &Materials() const { return materials_; }
  const Node &RootNode() const { return rootNode_; }
  const std::vector<DrawItem> &DrawItems() const { return drawItems_; }

  const std::string &SourceInputPath() const {
    return sourceInputPath_;
  } // LoadModelに渡された文字列
  const std::string &SourceFilePath() const {
    return sourceFilePath_;
  } // 実際に読んだファイル

private:
  struct VB {
    ID3D12Resource *resource = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view{};
    uint32_t vertexCount = 0;
  };

  struct SubmeshRange {
    uint32_t vertexStart = 0;
    uint32_t vertexCount = 0;
    uint32_t materialIndex = 0;
  };

  ID3D12Device *device_ = nullptr;
  VB vb_{};

  // 互換用：最初に見つかったテクスチャを入れる
  MaterialData materialFile_{};

  // assimp 解析結果
  std::vector<MaterialData> materials_;
  std::vector<SubmeshRange> submeshes_; // meshIndex -> range
  Node rootNode_{};
  std::vector<DrawItem> drawItems_;

  // ------------------------------------------------------------
  // 読み込み（assimp）
  // ------------------------------------------------------------
  bool LoadAssimp_(const std::string &filePath);

  bool ExtractScene_(const aiScene *scene, const std::string &baseDir,
                     std::vector<VertexData> &outVertices);

  // Node解析（再帰）
  Node ReadNode_(const aiNode *node) const;
  void BuildDrawItems_(const Node &node, const RC::Matrix4x4 &parentWorld);

  // Material（テクスチャパス）
  std::string ResolveTexturePath_(const aiScene *scene, const aiMaterial *mat,
                                  const std::string &baseDir) const;
  std::string ExtractEmbeddedTexture_(const aiScene *scene,
                                      const aiString &assimpTexPath,
                                      const std::string &baseDir) const;

  void UploadVB_(const std::vector<VertexData> &vertices);

  // Materialの index を「使っている分だけ」に詰める。
  // - OBJ で mMaterials[0] が空のダミーになるケースなどを吸収
  // - SubmeshRange / DrawItem の materialIndex も 0..N-1 に揃える
  void CompactMaterials_();

  // 右手→左手のための座標系変換（X反転）
  static RC::Matrix4x4 MakeAxisFlipX_();
  static RC::Matrix4x4 ConvertNodeMatrixRHtoLH_(const RC::Matrix4x4 &m);

  std::string sourceInputPath_;
  std::string sourceFilePath_;
};
