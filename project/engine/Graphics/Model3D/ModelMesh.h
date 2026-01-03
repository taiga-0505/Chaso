#pragma once
#include "Math/Math.h"
#include "Math/MathTypes.h"
#include "function/function.h"
#include "struct.h"
#include <d3d12.h>
#include <filesystem>
#include <string>
#include <vector>

// 見た目（共有）：VB / OBJ+MTLの読み込み結果だけを持つ
class ModelMesh {
public:
  ModelMesh() = default;
  ~ModelMesh();

  // objPath: .obj 直指定 or ディレクトリ指定（中から .obj を探す）
  bool LoadObj(ID3D12Device *device, const std::string &objPath);

  // UV未設定(=0,0)に対し簡易的な球面UVを焼き込む
  void EnsureSphericalUVIfMissing();

  bool Ready() const { return vb_.resource != nullptr && vb_.vertexCount > 0; }
  const D3D12_VERTEX_BUFFER_VIEW &VBV() const { return vb_.view; }
  uint32_t VertexCount() const { return vb_.vertexCount; }

  const MaterialData &MaterialFile() const { return materialFile_; }

private:
  struct VB {
    ID3D12Resource *resource = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view{};
    uint32_t vertexCount = 0;
  };

  ID3D12Device *device_ = nullptr;
  VB vb_{};
  MaterialData materialFile_{};

  bool LoadObjGeometryLikeFunction_(const std::string &directoryPath,
                                    const std::string &filename);

  bool LoadObjToVertices_(const std::string &directoryPath,
                          const std::string &filename,
                          std::vector<VertexData> &outVertices,
                          MaterialData &outMtl);

  void UploadVB_(const std::vector<VertexData> &vertices);

  MaterialData LoadMaterialTemplateFile_(const std::string &directoryPath,
                                         const std::string &filename);
};
