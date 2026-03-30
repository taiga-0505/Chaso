#include "Sprite2D.h"
#include "imgui/imgui.h"
#include <cassert>
#include <string>

using namespace RC;

Sprite2D::~Sprite2D() { Release(); }

void Sprite2D::Release() {
  mesh_.reset();

  if (cbWVP_.res) {
    cbWVP_.res.Reset();
    cbWVP_.map = nullptr;
  }
  if (cbMat_.res) {
    cbMat_.res.Reset();
    cbMat_.map = nullptr;
  }
}

void Sprite2D::Initialize(ID3D12Device *device,
                          const std::shared_ptr<SpriteMesh2D> &mesh,
                          float screenWidth, float screenHeight) {
  Release();
  device_ = device;
  mesh_ = mesh;
  screenW_ = screenWidth;
  screenH_ = screenHeight;

  // CB: WVP
  cbWVP_.res =
      CreateBufferResource(device_.Get(), sizeof(TransformationMatrix));
  cbWVP_.res->Map(0, nullptr, reinterpret_cast<void **>(&cbWVP_.map));
  cbWVP_.map->World = MakeIdentity4x4();
  cbWVP_.map->WVP = MakeIdentity4x4();

  // CB: Material
  cbMat_.res = CreateBufferResource(device_.Get(), sizeof(SpriteMaterial));
  cbMat_.res->Map(0, nullptr, reinterpret_cast<void **>(&cbMat_.map));
  cbMat_.map->color = {1, 1, 1, 1};
  cbMat_.map->uvTransform = MakeIdentity4x4();

  // ビュー/プロジェクション（左上基準の直交）
  view_ = MakeIdentity4x4();
  proj_ = MakeOrthographicMatrix(0.0f, 0.0f, screenW_, screenH_, 0.0f, 100.0f);

  SetSize(transform_.scale.x, transform_.scale.y);
}

void Sprite2D::SetScreenSize(float w, float h) {
  screenW_ = w;
  screenH_ = h;
  proj_ = MakeOrthographicMatrix(0.0f, 0.0f, screenW_, screenH_, 0.0f, 100.0f);
}

void Sprite2D::SetSize(float w, float h) {
  transform_.scale.x = w;
  transform_.scale.y = h;
  transform_.scale.z = 1.0f;
}

void Sprite2D::Update() {
  Matrix4x4 world = MakeAffineMatrix(transform_.scale, transform_.rotation,
                                     transform_.translation);
  cbWVP_.map->World = world;
  cbWVP_.map->WVP = Multiply(world, Multiply(view_, proj_));
}

void Sprite2D::Draw(ID3D12GraphicsCommandList *cmdList) const {
  if (!visible_)
    return;
  if (!mesh_ || !mesh_->Ready())
    return;

  assert(cmdList);

  const auto &vbv = mesh_->VBV();
  const auto &ibv = mesh_->IBV();

  cmdList->IASetVertexBuffers(0, 1, &vbv);
  cmdList->IASetIndexBuffer(&ibv);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // RootParam: 0=Material, 1=WVP, 2=SRV
  cmdList->SetGraphicsRootConstantBufferView(
      0, cbMat_.res->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbWVP_.res->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, srv_);

  cmdList->DrawIndexedInstanced(mesh_->IndexCount(), 1, 0, 0, 0);
}

void Sprite2D::DrawImGui(const char *name) {
#if RC_ENABLE_IMGUI


  std::string label = name ? std::string(name) : std::string("Sprite2D");
  if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    return;

  // 表示
  if (ImGui::Checkbox((std::string("表示##") + label).c_str(), &visible_)) {
  }

  if (visible_) {
    // Transform
    float pos[3] = {transform_.translation.x, transform_.translation.y,
                    transform_.translation.z};
    if (ImGui::DragFloat3((std::string("位置##") + label).c_str(), pos, 1.0f,
                          0.0f, 0.0f, "%f")) {
      transform_.translation = {pos[0], pos[1], pos[2]};
    }

    float rot[3] = {transform_.rotation.x, transform_.rotation.y,
                    transform_.rotation.z};
    if (ImGui::DragFloat3((std::string("回転##") + label).c_str(), rot, 0.01f,
                          0.0f, 0.0f, "%f")) {
      transform_.rotation = {rot[0], rot[1], rot[2]};
    }

    float size[2] = {transform_.scale.x, transform_.scale.y};
    if (ImGui::DragFloat2((std::string("サイズ##") + label).c_str(), size, 1.0f,
                          0.0f, 100000.0f, "%f")) {
      SetSize(size[0], size[1]);
    }

    // 色
    if (cbMat_.map) {
      float col[4] = {cbMat_.map->color.x, cbMat_.map->color.y,
                      cbMat_.map->color.z, cbMat_.map->color.w};
      if (ImGui::ColorEdit4((std::string("色##") + label).c_str(), col)) {
        cbMat_.map->color = {col[0], col[1], col[2], col[3]};
      }
    }

    ImGui::Text("SRV ptr: 0x%llx", (unsigned long long)srv_.ptr);
  }

#endif
}
