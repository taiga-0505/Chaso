#include "ModelMesh.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sstream>

using namespace RC;
namespace fs = std::filesystem;

static constexpr float kPi = std::numbers::pi_v<float>;

ModelMesh::~ModelMesh() {
  if (vb_.resource) {
    vb_.resource->Release();
    vb_.resource = nullptr;
  }
}

static bool IsSupportedModelExt_(std::string ext) {
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return (ext == ".obj" || ext == ".gltf" || ext == ".glb");
}

bool ModelMesh::LoadObj(ID3D12Device *device, const std::string &objPath) {
  device_ = device;

  rootNode_ = {};
  rootNode_.localMatrix = MakeIdentity4x4();
  rootNode_.name = "Root";
  rootNode_.children.clear();

  auto TryLoad = [&](const std::string &dir, const std::string &file) -> bool {
    if (!LoadObjGeometryLikeFunction_(dir, file))
      return false;
    EnsureSphericalUVIfMissing();
    return true;
  };

  fs::path p(objPath);

  if (fs::is_regular_file(p)) {
    if (!IsSupportedModelExt_(p.extension().string()))
      return false;
    return TryLoad(p.parent_path().string(), p.filename().string());
  }

  if (fs::is_directory(p)) {
    for (auto &entry : fs::directory_iterator(p)) {
      if (!entry.is_regular_file())
        continue;
      if (!IsSupportedModelExt_(entry.path().extension().string()))
        continue;

      return TryLoad(entry.path().parent_path().string(),
                     entry.path().filename().string());
    }
    return false;
  }

  return false;
}

bool ModelMesh::LoadObjGeometryLikeFunction_(const std::string &directoryPath,
                                             const std::string &filename) {
  std::vector<VertexData> verts;
  MaterialData mtl;

  // まずassimp（.obj/.gltf/.glb 全部これで行ける）
  if (!LoadObjToVertices_Assimp_(directoryPath, filename, verts, mtl)) {

    // assimp失敗時、.objだけ旧OBJパーサで保険
    std::string ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext != ".obj")
      return false;

    if (!LoadObjToVertices_(directoryPath, filename, verts, mtl))
      return false;
  }

  materialFile_ = mtl;
  UploadVB_(verts);
  return true;
}

void ModelMesh::EnsureSphericalUVIfMissing() {
  if (!vb_.resource || vb_.vertexCount == 0)
    return;

  VertexData *vtx = nullptr;
  vb_.resource->Map(0, nullptr, reinterpret_cast<void **>(&vtx));
  if (!vtx)
    return;

  // (0,0)しか無いなら焼く
  bool allZero = true;
  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    if (std::fabs(vtx[i].texcoord.x) > 1e-6f ||
        std::fabs(vtx[i].texcoord.y) > 1e-6f) {
      allZero = false;
      break;
    }
  }
  if (!allZero) {
    vb_.resource->Unmap(0, nullptr);
    return;
  }

  // 球面UV（ざっくり）
  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    const Vector3 p{vtx[i].position.x, vtx[i].position.y, vtx[i].position.z};
    const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    Vector3 n = (len > 1e-6f) ? Vector3{p.x / len, p.y / len, p.z / len}
                              : Vector3{0, 1, 0};

    float u = 0.5f + std::atan2(n.z, n.x) / (2.0f * kPi);
    float v = 0.5f - std::asin(n.y) / kPi;
    vtx[i].texcoord = {u, v};
  }

  vb_.resource->Unmap(0, nullptr);
}

bool ModelMesh::LoadObjToVertices_(const std::string &directoryPath,
                                   const std::string &filename,
                                   std::vector<VertexData> &outVertices,
                                   MaterialData &outMtl) {
  std::vector<Vector4> positions;
  std::vector<Vector3> normals;
  std::vector<Vector2> texcoords;

  std::ifstream file(directoryPath + "/" + filename);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    std::string id;
    std::istringstream s(line);
    s >> id;

    if (id == "v") {
      Vector4 p{};
      s >> p.x >> p.y >> p.z;
      p.x = -p.x;
      p.w = 1.0f;
      positions.push_back(p); // 左手系へ
    } else if (id == "vt") {
      Vector2 t{};
      s >> t.x >> t.y;
      t.y = 1.0f - t.y;
      texcoords.push_back(t);
    } else if (id == "vn") {
      Vector3 n{};
      s >> n.x >> n.y >> n.z;
      n.x = -n.x;
      normals.push_back(n);
    } else if (id == "f") {
      VertexData tri[3]{};

      for (int k = 0; k < 3; ++k) {
        std::string vd;
        s >> vd;
        std::string idx[3] = {"", "", ""};
        size_t prev = 0, pos;
        int field = 0;
        while (field < 2 && (pos = vd.find('/', prev)) != std::string::npos) {
          idx[field++] = vd.substr(prev, pos - prev);
          prev = pos + 1;
        }
        if (prev < vd.size() && field < 3)
          idx[field++] = vd.substr(prev);

        int pi = std::stoi(idx[0]);
        int ti = (!idx[1].empty()) ? std::stoi(idx[1]) : 0;
        int ni = (!idx[2].empty()) ? std::stoi(idx[2]) : 0;

        // OBJは1始まり
        tri[k].position = positions[pi - 1];
        tri[k].texcoord = (ti > 0) ? texcoords[ti - 1] : Vector2{0, 0};
        tri[k].normal = (ni > 0) ? normals[ni - 1] : Vector3{0, 1, 0};
      }

      outVertices.push_back(tri[0]);
      outVertices.push_back(tri[2]);
      outVertices.push_back(tri[1]);
    } else if (id == "mtllib") {
      std::string mtlName;
      s >> mtlName;
      outMtl = LoadMaterialTemplateFile_(directoryPath, mtlName);
    }
  }
  return true;
}

bool ModelMesh::LoadObjToVertices_Assimp_(const std::string &directoryPath,
                                          const std::string &filename,
                                          std::vector<VertexData> &outVertices,
                                          MaterialData &outMtl) {
  Assimp::Importer importer;

  const fs::path objPath = fs::path(directoryPath) / filename;

  std::string ext = objPath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  unsigned int flags = aiProcess_Triangulate | aiProcess_FlipWindingOrder;

  // OBJだけ v を 1-v にしたい
  if (ext == ".obj") {
    flags |= aiProcess_FlipUVs;
  }

  // gltf/glb は FlipUVs しない
  const aiScene *scene = importer.ReadFile(objPath.string(), flags);

  // ReadFile 成功後
  if (scene->mRootNode) {
    rootNode_ = ReadNode(scene->mRootNode);
  } else {
    rootNode_ = {};
    rootNode_.localMatrix = MakeIdentity4x4();
    rootNode_.name = "Root";
    rootNode_.children.clear();
  }

  outVertices.clear();
  outMtl.textureFilePath.clear();

  // Mesh解析
  for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
    aiMesh *mesh = scene->mMeshes[meshIndex];
    if (!mesh)
      continue;

    const bool hasNormals = mesh->HasNormals();
    const bool hasUV0 = mesh->HasTextureCoords(0);

    // Face解析（Triangulateしてるので基本3になる）
    for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
      const aiFace &face = mesh->mFaces[faceIndex];
      if (face.mNumIndices != 3) {
        // 念のため（Triangulateしてればここは基本通らない）
        continue;
      }

      // Vertex解析
      for (uint32_t e = 0; e < 3; ++e) {
        const uint32_t vIdx = face.mIndices[e];

        const aiVector3D &p = mesh->mVertices[vIdx];
        VertexData v{};
        v.position = {p.x, p.y, p.z, 1.0f};

        if (hasNormals) {
          const aiVector3D &n = mesh->mNormals[vIdx];
          v.normal = {n.x, n.y, n.z};
        } else {
          v.normal = {0.0f, 1.0f, 0.0f};
        }

        if (hasUV0) {
          const aiVector3D &uv = mesh->mTextureCoords[0][vIdx];
          v.texcoord = {uv.x, uv.y}; // FlipUVs 済みなのでここで反転しない
        } else {
          v.texcoord = {0.0f, 0.0f};
        }

        // いま君のOBJ手動ローダと同じ「x反転」で左手系化
        v.position.x *= -1.0f;
        v.normal.x *= -1.0f;

        outVertices.push_back(v);
      }
    }

    // Material解析（このModelMeshは material
    // 1個しか持てないので「最初に見つかったdiffuse」を採用）
    if (outMtl.textureFilePath.empty() &&
        mesh->mMaterialIndex < scene->mNumMaterials) {
      aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];
      if (mat && mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
          fs::path t(texPath.C_Str());
          if (t.is_absolute()) {
            outMtl.textureFilePath = t.string();
          } else {
            outMtl.textureFilePath =
                (fs::path(directoryPath) / t).lexically_normal().string();
          }
        }
      }
    }
  }

  return !outVertices.empty();
}

static Matrix4x4 MakeFlipX_() {
  Matrix4x4 s = MakeIdentity4x4();
  s.m[0][0] = -1.0f;
  return s;
}

// M' = S * M * S
static Matrix4x4 ConjugateFlipX_(const Matrix4x4 &m) {
  const Matrix4x4 s = MakeFlipX_();
  return Multiply(Multiply(s, m), s);
}

Node ModelMesh::ReadNode(aiNode *node) {
  Node result{};
  result.localMatrix = MakeIdentity4x4();

  aiMatrix4x4 aiLocal = node->mTransformation;

  // ここは君の流儀に合わせて transpose / コピー
  aiLocal.Transpose(); // ← 今まで通りでOKならそのまま

  Matrix4x4 local = *(reinterpret_cast<Matrix4x4 *>(&aiLocal));

  // 頂点でX反転してるなら、Node行列もX反転で共役する
  local = ConjugateFlipX_(local);

  result.localMatrix = local;
  result.name = node->mName.C_Str();
  result.children.resize(node->mNumChildren);

  for (uint32_t i = 0; i < node->mNumChildren; ++i) {
    result.children[i] = ReadNode(node->mChildren[i]);
  }
  return result;
}


void ModelMesh::UploadVB_(const std::vector<VertexData> &vertices) {
  vb_.vertexCount = static_cast<uint32_t>(vertices.size());
  if (vb_.vertexCount == 0)
    return;

  const size_t sizeBytes = sizeof(VertexData) * vb_.vertexCount;

  if (vb_.resource) {
    vb_.resource->Release();
    vb_.resource = nullptr;
  }

  vb_.resource = CreateBufferResource(device_, sizeBytes);

  void *mapped = nullptr;
  vb_.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, vertices.data(), sizeBytes);
  vb_.resource->Unmap(0, nullptr);

  vb_.view.BufferLocation = vb_.resource->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = static_cast<UINT>(sizeBytes);
  vb_.view.StrideInBytes = sizeof(VertexData);
}

MaterialData
ModelMesh::LoadMaterialTemplateFile_(const std::string &directoryPath,
                                     const std::string &filename) {
  MaterialData materialData{};
  std::string line;
  std::ifstream file(directoryPath + "/" + filename);
  assert(file.is_open());

  while (std::getline(file, line)) {
    std::string identifier;
    std::istringstream s(line);

    s >> identifier;
    if (identifier == "map_Kd") {
      std::string textureFilename;
      s >> textureFilename;
      materialData.textureFilePath = directoryPath + "/" + textureFilename;
    }
  }
  return materialData;
}
