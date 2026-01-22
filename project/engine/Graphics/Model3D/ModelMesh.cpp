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

bool ModelMesh::LoadObj(ID3D12Device *device, const std::string &objPath) {
  device_ = device;

  auto TryLoad = [&](const std::string &dir, const std::string &file) -> bool {
    if (!LoadObjGeometryLikeFunction_(dir, file)) {
      return false;
    }
    // UVが全部(0,0)なら簡易球面UVを焼く
    EnsureSphericalUVIfMissing();
    return true;
  };

  fs::path p(objPath);
  if (fs::is_regular_file(p)) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".obj") {
      return TryLoad(p.parent_path().string(), p.filename().string());
    }
    return false;
  }

  if (fs::is_directory(p)) {
    for (auto &entry : fs::directory_iterator(p)) {
      if (!entry.is_regular_file())
        continue;
      auto ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (ext == ".obj") {
        return TryLoad(entry.path().parent_path().string(),
                       entry.path().filename().string());
      }
    }
    return false;
  }

  return false;
}

bool ModelMesh::LoadObjGeometryLikeFunction_(const std::string &directoryPath,
                                             const std::string &filename) {
  std::vector<VertexData> verts;
  MaterialData mtl;
  if (!LoadObjToVertices_(directoryPath, filename, verts, mtl)) {
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
