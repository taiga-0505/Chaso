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

  // WVP CB（単発用：互換のため残す）
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
  // overrideを解除して materialIndexごとのSRVを使う
  textureSrv_ = {};
  materialSrvs_.clear();
  EnsureMaterialSrvsLoaded_();
}

void ModelObject::SetColor(const Vector4 &color) {
  if (cbMat_.mapped) {
    cbMat_.mapped->color = color;
  }
}

void ModelObject::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  cachedView_ = view;
  cachedProj_ = proj;
  hasVP_ = true;

  // 互換：一応 root（モデル全体）として更新しておく
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWvp_.mapped->World = world;
  cbWvp_.mapped->WVP = Multiply(world, Multiply(view, proj));
  cbWvp_.mapped->worldInverseTranspose = Transpose(Inverse(world));
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

void ModelObject::EnsureMaterialSrvsLoaded_() {
  if (!texman_ || !mesh_) {
    return;
  }

  const auto &mats = mesh_->Materials();
  if (mats.empty()) {
    // 互換：昔の1枚だけ
    materialSrvs_.clear();
    const auto &mtl = mesh_->MaterialFile();
    if (!mtl.textureFilePath.empty()) {
      materialSrvs_.resize(1);
      materialSrvs_[0] = texman_->Load(mtl.textureFilePath, /*srgb=*/true);
    }
    return;
  }

  if (!materialSrvs_.empty() && materialSrvs_.size() == mats.size()) {
    return; // 既にロード済み
  }

  materialSrvs_.assign(mats.size(), D3D12_GPU_DESCRIPTOR_HANDLE{});
  for (uint32_t i = 0; i < mats.size(); ++i) {
    if (mats[i].textureFilePath.empty())
      continue;

    const D3D12_GPU_DESCRIPTOR_HANDLE srv =
        texman_->Load(mats[i].textureFilePath, /*srgb=*/true);
    materialSrvs_[i] = (srv.ptr == 0) ? D3D12_GPU_DESCRIPTOR_HANDLE{} : srv;
  }
}

D3D12_GPU_DESCRIPTOR_HANDLE
ModelObject::GetSrvForMaterial_(uint32_t materialIndex) const {
  if (!materialSrvs_.empty() && materialIndex < materialSrvs_.size() &&
      materialSrvs_[materialIndex].ptr != 0) {
    return materialSrvs_[materialIndex];
  }

  // fallback：先頭の有効SRV
  for (const auto &h : materialSrvs_) {
    if (h.ptr != 0)
      return h;
  }

  return D3D12_GPU_DESCRIPTOR_HANDLE{};
}

void ModelObject::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!mesh_ || !mesh_->Ready() || !visible_)
    return;

  // Node階層を使う場合は DrawItem を使う
  const auto &items = mesh_->DrawItems();

  // テクスチャ（overrideが無い場合だけ materialIndex対応を準備）
  if (textureSrv_.ptr == 0) {
    EnsureMaterialSrvsLoaded_();
  }

  const auto &vbv = mesh_->VBV();
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.resource->GetGPUVirtualAddress());

  // Light CB（b1）: 外部ライトが指定されていればそちらを使う
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      (externalLightCBAddress_ != 0)
          ? externalLightCBAddress_
          : cbLight_.resource->GetGPUVirtualAddress();
  cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

  // View/Proj が無いなら単位行列で描く（事故防止）
  const Matrix4x4 view = hasVP_ ? cachedView_ : MakeIdentity4x4();
  const Matrix4x4 proj = hasVP_ ? cachedProj_ : MakeIdentity4x4();

  // ObjectのWorld
  const Matrix4x4 objectWorld =
      MakeAffineMatrix(transform_.scale, transform_.rotation,
                       transform_.translation);

  // ---------------------------------------------------------
  // ★重要：同じCBアドレスに何回も上書きすると、全部最後の値になる
  // ので、DrawItem分のCB領域を確保して「GPUアドレスをずらして」描く
  // ---------------------------------------------------------
  cbWvpBatchHead_ = 0;
  const uint32_t oneSize = Align256(sizeof(TransformationMatrix));
  const uint32_t drawCount =
      items.empty() ? 1u : static_cast<uint32_t>(items.size());
  EnsureWvpBatchCapacity_(drawCount);

  if (items.empty()) {
    // 互換：DrawItemが無い場合は全頂点を1発
    TransformationMatrix tm{};
    tm.World = objectWorld;
    tm.WVP = Multiply(objectWorld, Multiply(view, proj));
    tm.worldInverseTranspose = Transpose(Inverse(objectWorld));

    auto *dst = reinterpret_cast<TransformationMatrix *>(cbWvpBatchMapped_);
    *dst = tm;

    const D3D12_GPU_VIRTUAL_ADDRESS addr =
        cbWvpBatch_->GetGPUVirtualAddress();
    cmdList->SetGraphicsRootConstantBufferView(1, addr);

    // texture
    cmdList->SetGraphicsRootDescriptorTable(
        2, (textureSrv_.ptr != 0) ? textureSrv_ : GetSrvForMaterial_(0));

    cmdList->DrawInstanced(mesh_->VertexCount(), 1, 0, 0);
    return;
  }

  for (uint32_t i = 0; i < items.size(); ++i) {
    const auto &it = items[i];

    // Node行列（モデル空間）→ ObjectWorld を掛ける
    const Matrix4x4 world = Multiply(it.nodeWorld, objectWorld);

    TransformationMatrix tm{};
    tm.World = world;
    tm.WVP = Multiply(world, Multiply(view, proj));
    tm.worldInverseTranspose = Transpose(Inverse(world));

    // CB書き込み
    auto *dst = reinterpret_cast<TransformationMatrix *>(cbWvpBatchMapped_ +
                                                         uint64_t(i) * oneSize);
    *dst = tm;

    // CBアドレスをずらす
    const D3D12_GPU_VIRTUAL_ADDRESS addr =
        cbWvpBatch_->GetGPUVirtualAddress() + uint64_t(i) * oneSize;
    cmdList->SetGraphicsRootConstantBufferView(1, addr);

    // テクスチャ（override優先）
    const D3D12_GPU_DESCRIPTOR_HANDLE srv =
        (textureSrv_.ptr != 0) ? textureSrv_ : GetSrvForMaterial_(it.materialIndex);
    cmdList->SetGraphicsRootDescriptorTable(2, srv);

    // mesh範囲だけ描画
    cmdList->DrawInstanced(it.vertexCount, 1, it.vertexStart, 0);
  }
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
  // Batchは1枚テクスチャ運用が多いけど、overrideが無い場合は material[0] を使う
  if (textureSrv_.ptr == 0) {
    EnsureMaterialSrvsLoaded_();
  }
  cmdList->SetGraphicsRootDescriptorTable(
      2, (textureSrv_.ptr != 0) ? textureSrv_ : GetSrvForMaterial_(0));

  // Light CB（b1）: 外部ライトが指定されていればそちらを使う
  const D3D12_GPU_VIRTUAL_ADDRESS lightAddr =
      (externalLightCBAddress_ != 0)
          ? externalLightCBAddress_
          : cbLight_.resource->GetGPUVirtualAddress();
  cmdList->SetGraphicsRootConstantBufferView(3, lightAddr);

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
                                                         uint64_t(dstIndex) * oneSize);
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

  // PS 側は Lambert / HalfLambert + Blinn-Phong 固定運用。
  // モードは 0..2 のみ使う。
  static const char *kModes[] = {"None", "Lambert", "HalfLambert"};
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

    if (cbMat_.mapped->lightingMode != None) {
      ImGui::DragFloat((std::string("Shininess##") + label).c_str(),
                       &cbMat_.mapped->shininess, 0.5f, 0.0f, 256.0f, "%.1f");
      ImGui::SameLine();
      ImGui::TextDisabled("(0で鏡面なし)");
    }
  }

  // テクスチャ情報
  if (texman_ && mesh_) {
    ImGui::TextUnformatted("テクスチャ情報");
    if (textureSrv_.ptr != 0) {
      ImGui::TextDisabled("override: (SetTexture)");
    } else {
      // material一覧
      const auto &mats = mesh_->Materials();
      if (!mats.empty()) {
        ImGui::TextDisabled("materials: %d", (int)mats.size());
        for (int i = 0; i < (int)mats.size() && i < 8; ++i) {
          ImGui::TextDisabled("[%d] %s", i, mats[i].textureFilePath.c_str());
        }
        if ((int)mats.size() > 8) {
          ImGui::TextDisabled("... (%d more)", (int)mats.size() - 8);
        }
      } else if (!mesh_->MaterialFile().textureFilePath.empty()) {
        ImGui::TextDisabled("mtl: %s", mesh_->MaterialFile().textureFilePath.c_str());
      } else {
        ImGui::TextDisabled("(no texture)");
      }
    }
  }
}
