#include "Sphere.h"
#include "Math/Math.h"
#include "imgui/imgui.h"
#include <cassert>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace RC;

Sphere::~Sphere() {
  if (vb_.resource)
    vb_.resource->Release();
  if (ib_.resource)
    ib_.resource->Release();
  if (cbWvp_.resource)
    cbWvp_.resource->Release();
  if (cbMat_.resource)
    cbMat_.resource->Release();
  if (cbLight_.resource)
    cbLight_.resource->Release();
}

void Sphere::Initialize(ID3D12Device *device, float radius, UINT sliceCount,
                        UINT stackCount, bool inward) {
  device_ = device;
  inward_ = inward;

  // メッシュ生成
  BuildGeometry(radius, sliceCount, stackCount, inward_);
  UploadVB_();
  UploadIB_();

  // CB: WVP
  cbWvp_.resource = CreateBufferResource(device_, sizeof(TransformationMatrix));
  cbWvp_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbWvp_.mapped));
  cbWvp_.mapped->WVP = MakeIdentity4x4();
  cbWvp_.mapped->World = MakeIdentity4x4();
  cbWvp_.mapped->worldInverseTranspose = MakeIdentity4x4();

  // CB: Material
  cbMat_.resource = CreateBufferResource(device_, sizeof(Material));
  cbMat_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->uvTransform = MakeIdentity4x4();
  cbMat_.mapped->lightingMode = 2; // HalfLambert 既定

  // CB: Light（球ごとに持つ）
  cbLight_.resource = CreateBufferResource(device_, sizeof(DirectionalLight));
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;
  cbMat_.mapped->shininess = 32.0f;

  if (inward_) {
    cbMat_.mapped->lightingMode = 0;
  }
}

void Sphere::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));
}

void Sphere::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!vb_.resource || !ib_.resource || !visible_)
    return;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetIndexBuffer(&ib_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam: 0:Material, 1:WVP, 2:SRV, 3:Light
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWvp_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  cmdList->DrawIndexedInstanced(ib_.indexCount, 1, 0, 0, 0);
}

void Sphere::DrawImGui(const char *name) {
  std::string label = name ? std::string(name) : std::string("Sphere");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;
  // ---- 表示ON/OFF（Model3Dと同様）----
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
  //// ---- Lighting ----
  //ImGui::TextUnformatted("Lighting");
  //if (cbMat_.mapped && cbLight_.mapped) {
  //  static const char *kModes[] = {"None", "Lambert", "HalfLambert"};
  //  int mode = cbMat_.mapped->lightingMode;
  //  if (ImGui::Combo((std::string("モード##") + label).c_str(), &mode, kModes,
  //                   IM_ARRAYSIZE(kModes))) {
  //    cbMat_.mapped->lightingMode = mode;
  //  }
  //  ImGui::ColorEdit3((std::string("光カラー##") + label).c_str(),
  //                    &cbLight_.mapped->color.x, ImGuiColorEditFlags_Float);
  //  bool dirChanged = ImGui::DragFloat3(
  //      (std::string("光方向(x,y,z)##") + label).c_str(),
  //      &cbLight_.mapped->direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
  //  if (dirChanged) {
  //    Vector3 d = cbLight_.mapped->direction;
  //    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  //    if (len > 1e-6f) {
  //      d.x /= len;
  //      d.y /= len;
  //      d.z /= len;
  //      cbLight_.mapped->direction = d;
  //    }
  //  }
  //  ImGui::DragFloat((std::string("強さ##") + label).c_str(),
  //                   &cbLight_.mapped->intensity, 0.01f, 0.0f, 16.0f, "%.2f");

  //} else {
  //  ImGui::TextDisabled("Light / Material CB not ready.");
  //}
}

void Sphere::BuildGeometry(float radius, UINT sliceCount, UINT stackCount,
                           bool inward) {
  vertices_.clear();
  indices_.clear();

  // 上極点
  vertices_.push_back({Vector4(0, +radius, 0, 1), Vector2(0.0f, 0.0f)});

  // 中間リング（緯度）
  for (UINT lat = 1; lat < stackCount; ++lat) {
    float phi = lat * (float(M_PI) / float(stackCount));
    float v = float(lat) / float(stackCount);

    for (UINT lon = 0; lon <= sliceCount; ++lon) {
      float theta = lon * (2.0f * float(M_PI) / float(sliceCount));
      float u = float(lon) / float(sliceCount);

      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi);
      float z = radius * sinf(phi) * sinf(theta);

      vertices_.push_back({Vector4(x, y, z, 1), Vector2(u, v)});
    }
  }

  // 下極点
  vertices_.push_back({Vector4(0, -radius, 0, 1), Vector2(0.0f, 1.0f)});

  // 法線（位置の正規化）
  for (auto &v : vertices_) {
    float len =
        std::sqrt(v.position.x * v.position.x + v.position.y * v.position.y +
                  v.position.z * v.position.z);
    if (len > 0.0f) {
      v.normal.x = v.position.x / len;
      v.normal.y = v.position.y / len;
      v.normal.z = v.position.z / len;
    } else {
      v.normal = {0.0f, 1.0f, 0.0f};
    }
  }

  // 内向き指定なら「法線反転」＋「全トライアングルの巻き順反転」
  if (inward) {
    // 法線を内向きに
    for (auto &v : vertices_) {
      v.normal.x = -v.normal.x;
      v.normal.y = -v.normal.y;
      v.normal.z = -v.normal.z;
    }
  }

  // インデックス
  UINT ringVerts = sliceCount + 1;

  // 上極ファン（i と i+1 を入れ替え）
  for (UINT i = 1; i <= sliceCount; ++i) {
    indices_.push_back(0);
    indices_.push_back(i + 1);
    indices_.push_back(i);
  }

  // 中間帯のクアッド
  for (UINT i = 0; i < stackCount - 2; ++i) {
    for (UINT j = 0; j < sliceCount; ++j) {
      UINT a = 1 + i * ringVerts + j;
      UINT b = 1 + i * ringVerts + j + 1;
      UINT c = 1 + (i + 1) * ringVerts + j;
      UINT d = 1 + (i + 1) * ringVerts + j + 1;
      indices_.push_back(a);
      indices_.push_back(b);
      indices_.push_back(d);
      indices_.push_back(a);
      indices_.push_back(d);
      indices_.push_back(c);
    }
  }

  // 下極ファン
  UINT southPoleIndex = UINT(vertices_.size() - 1);
  UINT baseIndex = southPoleIndex - ringVerts;
  for (UINT i = 0; i < sliceCount; ++i) {
    indices_.push_back(southPoleIndex);
    indices_.push_back(baseIndex + i);
    indices_.push_back(baseIndex + i + 1);
  }

  if (inward) {
    for (size_t i = 0; i + 2 < indices_.size(); i += 3) {
      std::swap(indices_[i + 1], indices_[i + 2]);
    }
  }
}

void Sphere::UploadVB_() {
  vb_.vertexCount = static_cast<uint32_t>(vertices_.size());
  if (vb_.vertexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(VertexData) * vb_.vertexCount);
  vb_.resource = CreateBufferResource(device_, sizeBytes);

  void *mapped = nullptr;
  vb_.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, vertices_.data(), sizeBytes);
  vb_.resource->Unmap(0, nullptr);

  vb_.view.BufferLocation = vb_.resource->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = sizeBytes;
  vb_.view.StrideInBytes = sizeof(VertexData);
}

void Sphere::UploadIB_() {
  ib_.indexCount = static_cast<uint32_t>(indices_.size());
  if (ib_.indexCount == 0)
    return;

  const UINT sizeBytes = UINT(sizeof(uint16_t) * ib_.indexCount);
  // アップロードヒープでOK（頻繁に書き換えない・簡易）
  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC resDesc{};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = sizeBytes;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.Format = DXGI_FORMAT_UNKNOWN;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  HRESULT hr = device_->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ib_.resource));
  assert(SUCCEEDED(hr));

  uint16_t *mapped = nullptr;
  ib_.resource->Map(0, nullptr, reinterpret_cast<void **>(&mapped));
  std::memcpy(mapped, indices_.data(), sizeBytes);
  ib_.resource->Unmap(0, nullptr);

  ib_.view.BufferLocation = ib_.resource->GetGPUVirtualAddress();
  ib_.view.Format = DXGI_FORMAT_R16_UINT;
  ib_.view.SizeInBytes = sizeBytes;
}
