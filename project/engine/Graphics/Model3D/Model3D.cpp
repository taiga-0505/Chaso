#include "Model3D.h"
#include "Texture/TextureManager/TextureManager.h"
#include "imgui/imgui.h"
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

Model3D::~Model3D() {
  if (vb_.resource)
    vb_.resource->Release();
  if (cbWvp_.resource)
    cbWvp_.resource->Release();
  if (cbMat_.resource)
    cbMat_.resource->Release();
  if (cbLight_.resource)
    cbLight_.resource->Release();
  if (cbWvpBatch_) {
    cbWvpBatch_->Release();
    cbWvpBatch_ = nullptr;
  }
}

void Model3D::Initialize(ID3D12Device *device) {
  device_ = device;

  // WVP CB
  cbWvp_.resource = CreateBufferResource(device_, sizeof(TransformationMatrix));
  cbWvp_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbWvp_.mapped));
  cbWvp_.mapped->WVP = MakeIdentity4x4();
  cbWvp_.mapped->World = MakeIdentity4x4();

  // Material CB
  cbMat_.resource = CreateBufferResource(device_, sizeof(Material));
  cbMat_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->lightingMode = 2; // 既定 HalfLambert
  cbMat_.mapped->uvTransform = MakeIdentity4x4();

  // Light CB（各Modelが自前で持つ）
  cbLight_.resource = CreateBufferResource(device_, sizeof(DirectionalLight));
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;
  cbMat_.mapped->shininess = 32.0f;

  // 初期ライティング（コンストラクタで渡された値）を反映
  ApplyLightingIfReady_();
}

bool Model3D::LoadObjGeometryLikeFunction(const std::string &directoryPath,
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

bool Model3D::LoadObj(const std::string &objPath) {
  namespace fs = std::filesystem;

  auto TryLoadAndAutoBind = [&](const std::string &dir,
                                const std::string &file) -> bool {
    // まずジオメトリ＆.mtl を読む
    if (!LoadObjGeometryLikeFunction(dir, file))
      return false;

    // 条件: まだ外部から SetTexture されていない && .mtl に map_Kd がある &&
    // TextureManager が注入済み
    if (textureSrv_.ptr == 0 && !materialFile_.textureFilePath.empty() &&
        texman_) {
      fs::path texPath = materialFile_.textureFilePath; // 相対→絶対へ解決
      // map_Kd はアルベド想定なので sRGB=true
      D3D12_GPU_DESCRIPTOR_HANDLE srv =
          texman_->Load(texPath.string(), /*srgb=*/true);
      if (srv.ptr != 0) {
        textureSrv_ = srv;
      }
    }
    // -----------------------------------
    return true;
  };

  fs::path p(objPath);
  if (fs::is_regular_file(p)) {
    // 直接 .obj が指定されたケース
    if (p.extension() == ".obj") {
      return TryLoadAndAutoBind(p.parent_path().string(),
                                p.filename().string());
    } else {
      // 拡張子が .obj 以外なら失敗
      return false;
    }
  } else if (fs::is_directory(p)) {
    // ディレクトリが渡されたので中から .obj を探す
    for (auto &entry : fs::directory_iterator(p)) {
      if (entry.is_regular_file()) {
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".obj") {
          return TryLoadAndAutoBind(entry.path().parent_path().string(),
                                    entry.path().filename().string());
        }
      }
    }
    // 見つからなかった
    return false;
  }
  // どちらでもない
  return false;
}

void Model3D::EnsureSphericalUVIfMissing() {
  if (!vb_.resource || vb_.vertexCount == 0)
    return;

  VertexData *vtx = nullptr;
  vb_.resource->Map(0, nullptr, reinterpret_cast<void **>(&vtx));
  if (!vtx)
    return;

  bool allZero = true;
  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    if (std::abs(vtx[i].texcoord.x) > 1e-8f ||
        std::abs(vtx[i].texcoord.y) > 1e-8f) {
      allZero = false;
      break;
    }
  }
  if (!allZero) {
    vb_.resource->Unmap(0, nullptr);
    return;
  }

  for (uint32_t i = 0; i < vb_.vertexCount; ++i) {
    const float x = vtx[i].position.x;
    const float y = vtx[i].position.y;
    const float z = vtx[i].position.z;

    const float r = (std::max)(1e-6f, std::sqrt(x * x + y * y + z * z));
    const float theta = std::atan2(z, x); // [-pi, pi]
    const float phi =
        std::asin(std::clamp(y / r, -1.0f, 1.0f)); // [-pi/2, pi/2]

    float u = 0.5f + (theta / (2.0f * kPi));
    float v = 0.5f - (phi / (1.0f * kPi));

    if (u < 0.0f)
      u += 1.0f;
    if (u > 1.0f)
      u -= 1.0f;

    vtx[i].texcoord = {u, v};
  }

  vb_.resource->Unmap(0, nullptr);
  cbMat_.mapped->uvTransform = MakeIdentity4x4();
}

void Model3D::ResetTextureToMtl() {
  // TextureManager が未設定ならテクスチャを無効化して終了
  if (!texman_) {
    textureSrv_ = {};
    return;
  }

  // .mtl
  // にテクスチャ記述が無ければ無効化（チェック模様などデフォルトへ戻したい場合はここで設定）
  if (materialFile_.textureFilePath.empty()) {
    textureSrv_ = {};
    return;
  }

  // materialFile_.textureFilePath は LoadObj～ で  directoryPath + "/" +
  // textureFilename へ解決済み
  const fs::path texPath = materialFile_.textureFilePath;

  // map_Kd は sRGB 想定
  const D3D12_GPU_DESCRIPTOR_HANDLE srv =
      texman_->Load(texPath.string(), /*srgb=*/true);

  // 読み込みに失敗したら無効化、成功したらそのSRVへ
  if (srv.ptr == 0) {
    textureSrv_ = {};
  } else {
    textureSrv_ = srv;
  }
}

void Model3D::SetColor(const Vector4 &color) {
  if (cbMat_.mapped) {
    cbMat_.mapped->color = color;
  }
}

void Model3D::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
}

void Model3D::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!vb_.resource || !visible_)
    return;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWvp_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  cmdList->DrawInstanced(vb_.vertexCount, 1, 0, 0);
}

void Model3D::EnsureWvpBatchCapacity_(uint32_t count) {
  const uint32_t oneSize = Align256(sizeof(TransformationMatrix));
  if (cbWvpBatchCapacity_ >= count)
    return;

  if (cbWvpBatch_) {
    cbWvpBatch_->Release();
    cbWvpBatch_ = nullptr;
    cbWvpBatchMapped_ = nullptr;
  }

  cbWvpBatchCapacity_ =
      (std::max)(count, cbWvpBatchCapacity_ * 2u + 16u); // ちょい多め
  const uint64_t totalSize = uint64_t(oneSize) * cbWvpBatchCapacity_;
  cbWvpBatch_ = CreateBufferResource(device_, totalSize);
  cbWvpBatch_->Map(0, nullptr, reinterpret_cast<void **>(&cbWvpBatchMapped_));
}

void Model3D::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                        const Matrix4x4 &view, const Matrix4x4 &proj,
                        const std::vector<Transform> &instances) {
  if (!vb_.resource || instances.empty())
    return;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  const uint32_t oneSize = Align256(sizeof(TransformationMatrix));

  // ★必要容量は「今のカーソル＋今回個数」ぶん
  EnsureWvpBatchCapacity_(cbWvpBatchHead_ +
                          static_cast<uint32_t>(instances.size()));
  const uint32_t base = cbWvpBatchHead_;

  Transform bak = transform_;
  for (uint32_t i = 0; i < instances.size(); ++i) {
    const_cast<Transform &>(transform_) = instances[i];
    Update(view, proj);

    const uint32_t dstIndex = base + i; // 積み増し書き込み
    auto *dst = reinterpret_cast<TransformationMatrix *>(cbWvpBatchMapped_ +
                                                         dstIndex * oneSize);
    *dst = *cbWvp_.mapped;

    D3D12_GPU_VIRTUAL_ADDRESS addr =
        cbWvpBatch_->GetGPUVirtualAddress() + uint64_t(dstIndex) * oneSize;
    cmdList->SetGraphicsRootConstantBufferView(1, addr);
    cmdList->DrawInstanced(vb_.vertexCount, 1, 0, 0);
  }
  const_cast<Transform &>(transform_) = bak;

  // カーソル前進
  cbWvpBatchHead_ += static_cast<uint32_t>(instances.size());
}

// =========================
// 内部：OBJローダ
// =========================

bool Model3D::LoadObjToVertices_(const std::string &directoryPath,
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
      VertexData tri[3];
      for (int vi = 0; vi < 3; ++vi) {
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

        Vector4 p = positions[pi - 1];
        Vector2 t = (ti > 0 && ti - 1 < (int)texcoords.size())
                        ? texcoords[ti - 1]
                        : Vector2{0, 0};
        Vector3 n = (ni > 0 && ni - 1 < (int)normals.size()) ? normals[ni - 1]
                                                             : Vector3{0, 0, 0};
        tri[vi] = {p, t, n};
      }
      // 面の向きを反転して格納（既存実装と同じ）
      outVertices.push_back(tri[2]);
      outVertices.push_back(tri[1]);
      outVertices.push_back(tri[0]);
    } else if (id == "mtllib") {
      std::string mtlName;
      s >> mtlName;
      outMtl = LoadMaterialTemplateFile_(directoryPath, mtlName);
    }
  }
  return true;
}

void Model3D::UploadVB_(const std::vector<VertexData> &vertices) {
  vb_.vertexCount = static_cast<uint32_t>(vertices.size());
  if (vb_.vertexCount == 0)
    return;

  const size_t sizeBytes = sizeof(VertexData) * vb_.vertexCount;
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
Model3D::LoadMaterialTemplateFile_(const std::string &directoryPath,
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

// -------------------------------
// ライティング設定 4 引数版
// -------------------------------
Model3D &Model3D::SetLightingConfig(LightingMode mode,
                                    const std::array<float, 3> &color,
                                    const std::array<float, 3> &dir,
                                    float intensity) {
  initialLighting_.mode = mode;
  initialLighting_.color[0] = color[0];
  initialLighting_.color[1] = color[1];
  initialLighting_.color[2] = color[2];

  // ※必要ならここで正規化
  initialLighting_.dir[0] = dir[0];
  initialLighting_.dir[1] = dir[1];
  initialLighting_.dir[2] = dir[2];

  initialLighting_.intensity = intensity;

  ApplyLightingIfReady_();
  return *this;
}

void Model3D::ApplyLightingIfReady_() {
  if (cbMat_.mapped) {
    cbMat_.mapped->lightingMode = static_cast<int>(initialLighting_.mode);
  }
  if (cbLight_.mapped) {
    cbLight_.mapped->color = {initialLighting_.color[0],
                              initialLighting_.color[1],
                              initialLighting_.color[2], 1.0f};
    cbLight_.mapped->direction = {initialLighting_.dir[0],
                                  initialLighting_.dir[1],
                                  initialLighting_.dir[2]};
    cbLight_.mapped->intensity = initialLighting_.intensity;
  }
}

void Model3D::DrawImGui(const char *name, bool showLightingUi) {
  std::string label = name ? std::string(name) : std::string("Model3D");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;

  // ---- 表示ON/OFF ----
  bool vis = Visible();
  if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &vis))
    SetVisible(vis);

  // ---- Transform ----
  ImGui::TextUnformatted("Transform");
  ImGui::DragFloat3((std::string("位置(x,y,z)##") + label).c_str(),
                    &transform_.translation.x, 0.1f, -4096.0f, 4096.0f, "%.2f");
  ImGui::SliderAngle((std::string("回転X##") + label).c_str(),
                     &transform_.rotation.x);
  ImGui::SliderAngle((std::string("回転Y##") + label).c_str(),
                     &transform_.rotation.y);
  ImGui::SliderAngle((std::string("回転Z##") + label).c_str(),
                     &transform_.rotation.z);
  ImGui::DragFloat3((std::string("スケール(x,y,z)##") + label).c_str(),
                    &transform_.scale.x, 0.01f, 0.0001f, 1000.0f, "%.3f");

  if (ImGui::Button((std::string("Transformリセット##") + label).c_str())) {
    transform_.translation = {0, 0, 0};
    transform_.rotation = {0, 0, 0};
    transform_.scale = {1, 1, 1};
  }
  ImGui::Dummy(ImVec2(0, 6));

  // ---- Material（乗算カラー・UV行列）----
  ImGui::TextUnformatted("Material");
  if (cbMat_.mapped) {
    ImGui::ColorEdit4((std::string("カラー(乗算)##") + label).c_str(),
                      &cbMat_.mapped->color.x, ImGuiColorEditFlags_Float);

    // 簡易UVスケール＆平行移動（行列に焼く）
    static Vector3 uvS{1, 1, 1};
    static Vector3 uvT{0, 0, 0};
    bool updUV = false;
    updUV |= ImGui::DragFloat2((std::string("UV 平行移動##") + label).c_str(),
                               &uvT.x, 0.005f);
    updUV |= ImGui::DragFloat2((std::string("UV スケール##") + label).c_str(),
                               &uvS.x, 0.005f, 0.001f, 16.0f);
    if (updUV) {
      Matrix4x4 m = MakeIdentity4x4();
      m = Multiply(MakeScaleMatrix(uvS), m);
      m = Multiply(m, MakeTranslateMatrix(uvT));
      cbMat_.mapped->uvTransform = m;
    }

    if (ImGui::Button((std::string("UV行列リセット##") + label).c_str())) {
      cbMat_.mapped->uvTransform = MakeIdentity4x4();
      uvS = {1, 1, 1};
      uvT = {0, 0, 0};
    }
  } else {
    ImGui::TextDisabled("Material CB not ready.");
  }
  ImGui::Dummy(ImVec2(0, 6));

  if (showLightingUi) {
    // ---- Lighting ----
    ImGui::TextUnformatted("Lighting");
    if (cbMat_.mapped && cbLight_.mapped) {
      // ライティングモード（enumと同期）
      static const char *kModes[] = {"None", "Lambert", "HalfLambert"};
      int mode = cbMat_.mapped->lightingMode;
      if (ImGui::Combo((std::string("モード##") + label).c_str(), &mode, kModes,
                       IM_ARRAYSIZE(kModes))) {
        cbMat_.mapped->lightingMode = mode;
      }

      // 平行光（色・方向・強さ）
      ImGui::ColorEdit3((std::string("光カラー##") + label).c_str(),
                        &cbLight_.mapped->color.x, ImGuiColorEditFlags_Float);
      bool dirChanged = ImGui::DragFloat3(
          (std::string("光方向(x,y,z)##") + label).c_str(),
          &cbLight_.mapped->direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
      if (dirChanged) {
        Vector3 d = cbLight_.mapped->direction;
        float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len > 1e-6f) {
          d.x /= len;
          d.y /= len;
          d.z /= len;
          cbLight_.mapped->direction = d;
        }
      }
      ImGui::DragFloat((std::string("強さ##") + label).c_str(),
                       &cbLight_.mapped->intensity, 0.01f, 0.0f, 16.0f, "%.2f");
    } else {
      ImGui::TextDisabled("Light / Material CB not ready.");
    }
    ImGui::Dummy(ImVec2(0, 6));
  }
  // ---- メッシュ情報/便利ボタン ----

  // .mtl側の map_Kd を再バインド（TextureManagerがあれば）
  if (texman_ && !materialFile_.textureFilePath.empty()) {
    ImGui::TextUnformatted("テクスチャ情報");
    ImGui::TextDisabled("tex: %s", materialFile_.textureFilePath.c_str());
  }
}
