#include "ModelObject.h"
#include "Texture/TextureManager/TextureManager.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>

using namespace RC;
namespace fs = std::filesystem;

ModelObject::~ModelObject() {
  if (cbWvp_.resource)
    cbWvp_.resource->Release();
  if (cbMat_.resource)
    cbMat_.resource->Release();
  if (cbLight_.resource)
    cbLight_.resource->Release();
  if (cbWvpBatch_) {
    cbWvpBatch_->Release();
    cbWvpBatch_ = nullptr;
    cbWvpBatchMapped_ = nullptr;
  }
}

void ModelObject::Initialize(ID3D12Device *device) {
  device_ = device;

  // WVP CB
  cbWvp_.resource = CreateBufferResource(device_, sizeof(TransformationMatrix));
  cbWvp_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbWvp_.mapped));
  cbWvp_.mapped->WVP = MakeIdentity4x4();
  cbWvp_.mapped->World = MakeIdentity4x4();
  cbWvp_.mapped->worldInverseTranspose = MakeIdentity4x4();

  // Material CB
  cbMat_.resource = CreateBufferResource(device_, sizeof(Material));
  cbMat_.resource->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.mapped));
  cbMat_.mapped->color = {1, 1, 1, 1};
  cbMat_.mapped->lightingMode = 2; // 既定 HalfLambert
  cbMat_.mapped->uvTransform = MakeIdentity4x4();

  // Light CB（各Objectが自前で持つ）
  cbLight_.resource = CreateBufferResource(device_, sizeof(DirectionalLight));
  cbLight_.resource->Map(0, nullptr,
                         reinterpret_cast<void **>(&cbLight_.mapped));
  cbLight_.mapped->color = {1, 1, 1, 1};
  cbLight_.mapped->direction = {0.0f, -1.0f, 0.0f};
  cbLight_.mapped->intensity = 1.0f;
  cbMat_.mapped->shininess = 32.0f;

  // 初期ライティングを反映
  ApplyLightingIfReady_();
}

void ModelObject::EnsureSphericalUVIfMissing() {
  if (mesh_) {
    mesh_->EnsureSphericalUVIfMissing();
  }
}

void ModelObject::ResetTextureToMtl() {
  if (!texman_ || !mesh_) {
    textureSrv_ = {};
    return;
  }

  const auto &mtl = mesh_->MaterialFile();
  if (mtl.textureFilePath.empty()) {
    textureSrv_ = {};
    return;
  }

  const D3D12_GPU_DESCRIPTOR_HANDLE srv =
      texman_->Load(mtl.textureFilePath, /*srgb=*/true);

  textureSrv_ = (srv.ptr == 0) ? D3D12_GPU_DESCRIPTOR_HANDLE{} : srv;
}

void ModelObject::SetColor(const Vector4 &color) {
  if (cbMat_.mapped) {
    cbMat_.mapped->color = color;
  }
}

void ModelObject::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));
}

void ModelObject::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!mesh_ || !mesh_->Ready() || !visible_)
    return;

  const auto &vbv = mesh_->VBV();
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWvp_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  cmdList->DrawInstanced(mesh_->VertexCount(), 1, 0, 0);
}

void ModelObject::EnsureWvpBatchCapacity_(uint32_t count) {
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

void ModelObject::DrawBatch(ID3D12GraphicsCommandList *cmdList,
                            const Matrix4x4 &view, const Matrix4x4 &proj,
                            const std::vector<Transform> &instances) {
  if (!mesh_ || !mesh_->Ready() || instances.empty())
    return;

  const auto &vbv = mesh_->VBV();
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, textureSrv_);
  cmdList->SetGraphicsRootConstantBufferView(
      3, cbLight_.resource->GetGPUVirtualAddress());

  const uint32_t oneSize = Align256(sizeof(TransformationMatrix));
  EnsureWvpBatchCapacity_(cbWvpBatchHead_ +
                          static_cast<uint32_t>(instances.size()));
  const uint32_t base = cbWvpBatchHead_;

  for (uint32_t i = 0; i < instances.size(); ++i) {
    const Transform &tr = instances[i];

    Matrix4x4 world = MakeAffineMatrix(tr.scale, tr.rotation, tr.translation);

    TransformationMatrix tm{};
    tm.World = world;
    tm.WVP = Multiply(world, Multiply(view, proj));
    tm.worldInverseTranspose = Transpose(Inverse(world));

    const uint32_t dstIndex = base + i;
    auto *dst = reinterpret_cast<TransformationMatrix *>(cbWvpBatchMapped_ +
                                                         dstIndex * oneSize);
    *dst = tm;

    D3D12_GPU_VIRTUAL_ADDRESS addr =
        cbWvpBatch_->GetGPUVirtualAddress() + uint64_t(dstIndex) * oneSize;
    cmdList->SetGraphicsRootConstantBufferView(1, addr);
    cmdList->DrawInstanced(mesh_->VertexCount(), 1, 0, 0);
  }

  cbWvpBatchHead_ += static_cast<uint32_t>(instances.size());
}

ModelObject &ModelObject::SetLightingConfig(LightingMode mode,
                                            const std::array<float, 3> &color,
                                            const std::array<float, 3> &dir,
                                            float intensity) {
  initialLighting_.mode = mode;
  initialLighting_.color[0] = color[0];
  initialLighting_.color[1] = color[1];
  initialLighting_.color[2] = color[2];

  initialLighting_.dir[0] = dir[0];
  initialLighting_.dir[1] = dir[1];
  initialLighting_.dir[2] = dir[2];

  initialLighting_.intensity = intensity;

  ApplyLightingIfReady_();
  return *this;
}

void ModelObject::ApplyLightingIfReady_() {
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

void ModelObject::DrawImGui(const char *name, bool showLightingUi) {
  std::string label = name ? std::string(name) : std::string("ModelObject");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;

  bool vis = Visible();
  if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &vis))
    SetVisible(vis);

  static const char *kModes[] = {"None", "Lambert", "HalfLambert", "Phong", "Blinn-Phong"};
  int mode = cbMat_.mapped->lightingMode;
  if (ImGui::Combo((std::string("モード##") + label).c_str(), &mode, kModes,
                   IM_ARRAYSIZE(kModes))) {
    cbMat_.mapped->lightingMode = mode;
  }

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
                    &transform_.scale.x, 0.01f, 0.001f, 4096.0f, "%.3f");

  ImGui::Dummy(ImVec2(0, 6));

  if (showLightingUi) {
    if (cbLight_.mapped && cbMat_.mapped) {
      ImGui::TextUnformatted("Lighting / Material");
      ImGui::ColorEdit3((std::string("ライト色##") + label).c_str(),
                        &cbLight_.mapped->color.x, ImGuiColorEditFlags_Float);
      ImGui::DragFloat3((std::string("ライト方向##") + label).c_str(),
                        &cbLight_.mapped->direction.x, 0.01f, -1.0f, 1.0f);
      ImGui::DragFloat((std::string("強さ##") + label).c_str(),
                       &cbLight_.mapped->intensity, 0.01f, 0.0f, 64.0f);

      ImGui::Dummy(ImVec2(0, 4));
      ImGui::ColorEdit4((std::string("カラー(乗算)##") + label).c_str(),
                        &cbMat_.mapped->color.x, ImGuiColorEditFlags_Float);
    } else {
      ImGui::TextDisabled("Light / Material CB not ready.");
    }
    ImGui::Dummy(ImVec2(0, 6));

    if (cbMat_.mapped->lightingMode == 3) {
      ImGui::DragFloat((std::string("Shininess##") + label).c_str(),
                       &cbMat_.mapped->shininess, 1.0f, 1.0f, 256.0f, "%.0f");
    }
  }

  // .mtl側の map_Kd 情報
  if (texman_ && mesh_ && !mesh_->MaterialFile().textureFilePath.empty()) {
    ImGui::TextUnformatted("テクスチャ情報");
    ImGui::TextDisabled("tex: %s",
                        mesh_->MaterialFile().textureFilePath.c_str());
  }
}
