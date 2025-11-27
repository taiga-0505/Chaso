#include "Particle.h"
#include "Dx12Core.h"
#include "RenderCommon.h"
#include <Math/Math.h>
#include <cstdio>
#include <function/function.h>
#include <imgui/imgui.h>

namespace RC {

void Particle::Initialize(SceneContext &ctx) {

  device_ = ctx.core->GetDevice();
  sbMgr_ = &ctx.core->StructuredBuffers();

  // ImGui／挙動用のパラメータ初期化
  visible_ = true;
  enableUpdate_ = false;
  uvScale_ = {1.0f, 1.0f};
  uvTranslate_ = {0.0f, 0.0f};
  uvRotate_ = 0.0f;

  modelData.vertices.clear();
  // modelData.vertices.reserve(6);

  // 左上(-1,  1)  右上(1,  1)  左下(-1, -1)  右下(1, -1)
  // 1枚目: 左上 -> 右上 -> 左下 （時計回り）
  // 2枚目: 左下 -> 右上 -> 右下 （時計回り）

  modelData.vertices.push_back({.position = {-1.0f, 1.0f, 0.0f, 1.0f},
                                .texcoord = {0.0f, 0.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 左上

  modelData.vertices.push_back({.position = {1.0f, 1.0f, 0.0f, 1.0f},
                                .texcoord = {1.0f, 0.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 右上

  modelData.vertices.push_back({.position = {-1.0f, -1.0f, 0.0f, 1.0f},
                                .texcoord = {0.0f, 1.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 左下

  modelData.vertices.push_back({.position = {-1.0f, -1.0f, 0.0f, 1.0f},
                                .texcoord = {0.0f, 1.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 左下

  modelData.vertices.push_back({.position = {1.0f, 1.0f, 0.0f, 1.0f},
                                .texcoord = {1.0f, 0.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 右上

  modelData.vertices.push_back({.position = {1.0f, -1.0f, 0.0f, 1.0f},
                                .texcoord = {1.0f, 1.0f},
                                .normal = {0.0f, 0.0f, 1.0f}}); // 右下

  vertexCount_ = static_cast<uint32_t>(modelData.vertices.size());
  assert(vertexCount_ > 0);

  // 2) 頂点バッファ
  vbResource_ =
      CreateBufferResource(device_, sizeof(VertexData) * vertexCount_);
  VertexData *vbMapped = nullptr;
  vbResource_->Map(0, nullptr, reinterpret_cast<void **>(&vbMapped));
  std::memcpy(vbMapped, modelData.vertices.data(),
              sizeof(VertexData) * vertexCount_);
  vbResource_->Unmap(0, nullptr);

  vbView_.BufferLocation = vbResource_->GetGPUVirtualAddress();
  vbView_.StrideInBytes = sizeof(VertexData);
  vbView_.SizeInBytes = sizeof(VertexData) * vertexCount_;

  // ==========
  // 2) Instancing 用 StructuredBuffer
  // ==========
  instanceBufferId_ = sbMgr_->Create(sizeof(ParticleForGPU), instanceCount);
  instancingData_ =
      reinterpret_cast<ParticleForGPU *>(sbMgr_->Map(instanceBufferId_));
  instanceSrv_ = sbMgr_->GetSrv(instanceBufferId_);

  // Map して CPU から書き込めるようにしておく
  instancingData_ =
      static_cast<ParticleForGPU *>(sbMgr_->Map(instanceBufferId_));

  // 描画時に使う SRV
  instanceSrv_ = sbMgr_->GetSrv(instanceBufferId_);

  instancingResource =
      CreateBufferResource(device_, sizeof(ParticleForGPU) * instanceCount);

  // 4) Material CB（1個だけ） b0
  cbMat_ = CreateBufferResource(device_, Align256(sizeof(SpriteMaterial)));
  cbMat_->Map(0, nullptr, reinterpret_cast<void **>(&cbMatMapped_));
  *cbMatMapped_ = SpriteMaterial{}; // 一旦 0 クリア
  cbMatMapped_->color = {1, 1, 1, 1};
  cbMatMapped_->uvTransform = MakeIdentity4x4();
  cbMat_->Unmap(0, nullptr);
  cbMatMapped_ = nullptr;

  // テクスチャ
  int texHandle = RC::LoadTex("Resources/r.png", true);
  textureSrv_ = RC::GetSrv(texHandle);

  // ParticleData 初期化
  for (int i = 0; i < instanceCount; ++i) {

    particles[i] = MakeNewParticle(randomEngine);
  }
}

void Particle::Finalize() {

  if (sbMgr_ && instanceBufferId_ >= 0) {
    sbMgr_->Destroy(instanceBufferId_);
    instanceBufferId_ = -1;
    instancingData_ = nullptr;
    instanceSrv_ = {};
  }

  if (vbResource_) {
    vbResource_->Release();
    vbResource_ = nullptr;
  }

  if (cbMat_) {
    cbMat_->Unmap(0, nullptr);
    cbMat_->Release();
    cbMat_ = nullptr;
    cbMatMapped_ = nullptr;
  }

  device_ = nullptr;
  sbMgr_ = nullptr;
}

void Particle::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  if (!instancingData_) {
    return;
  }

  if (enableUpdate_) {
    for (int i = 0; i < instanceCount; ++i) {
      particles[i].transform += particles[i].velocity;
    }
  }

  for (int i = 0; i < instanceCount; ++i) {
    auto &p = particles[i];

    // Transform からワールド行列を作成
    const RC::Vector3 &s = p.transform.scale;
    const RC::Vector3 &r = p.transform.rotation;
    const RC::Vector3 &t = p.transform.translation;

    Matrix4x4 world = MakeAffineMatrix(s, r, t);

    ParticleForGPU wvp{};
    wvp.World = world;
    wvp.WVP = Multiply(world, Multiply(view, proj));
    wvp.color = p.color;

    instancingData_[i] = wvp;
  }
}

void Particle::Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl) {
  if (!visible_) {
    return;
  }

  if (!vbResource_ || instanceSrv_.ptr == 0 || vertexCount_ == 0) {
    return;
  }

  cl->SetGraphicsRootSignature(ctx.particlePSO->Root());
  cl->SetPipelineState(ctx.particlePSO->PSO());

  // IA 設定
  cl->IASetVertexBuffers(0, 1, &vbView_);
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // 0: Material (b0, PS)
  if (cbMat_) {
    cl->SetGraphicsRootConstantBufferView(0, cbMat_->GetGPUVirtualAddress());
  }

  // 1: インスタンス用 StructuredBuffer (t0, VS) の SRV テーブル
  cl->SetGraphicsRootDescriptorTable(1, instanceSrv_);

  // 2: テクスチャ (t1, PS) の SRV テーブル
  if (textureSrv_.ptr != 0) {
    cl->SetGraphicsRootDescriptorTable(2, textureSrv_);
  }

  cl->DrawInstanced(vertexCount_, instanceCount, 0, 0);
}

void Particle::DrawImGui() {

#ifdef _DEBUG
  if (ImGui::TreeNode("Particle")) {
    // 表示／非表示
    ImGui::Checkbox("Visible", &visible_);

    // Update を動かすかどうか
    ImGui::Checkbox("Enable Update", &enableUpdate_);

    // =======================
    // ★ 全パーティクル一括操作
    // =======================
    if (ImGui::TreeNode("All Particles")) {

      // 上書き用の値
      static RC::Vector3 allPos{0.0f, 0.0f, 0.0f};
      static RC::Vector3 allRot{0.0f, 0.0f, 0.0f};
      static RC::Vector3 allScale{1.0f, 1.0f, 1.0f};
      static RC::Vector3 allVel{0.0f, 0.0f, 0.0f};

      ImGui::TextUnformatted("↓この値で全部を上書き");
      ImGui::DragFloat3("All Position", &allPos.x, 0.01f);
      ImGui::DragFloat3("All Rotation", &allRot.x, 0.01f);
      ImGui::DragFloat3("All Scale", &allScale.x, 0.01f);
      ImGui::DragFloat3("All Velocity", &allVel.x, 0.001f);

      if (ImGui::Button("Apply To All")) {
        for (int i = 0; i < instanceCount; ++i) {
          particles[i].transform.translation = allPos;
          particles[i].transform.rotation = allRot;
          particles[i].transform.scale = allScale;
          particles[i].velocity = allVel;
        }
      }

      ImGui::Separator();

      // 位置だけオフセット加算したいとき用
      static RC::Vector3 addPos{0.0f, 0.0f, 0.0f};
      ImGui::DragFloat3("Add Position", &addPos.x, 0.01f);
      if (ImGui::Button("Offset All")) {
        for (int i = 0; i < instanceCount; ++i) {
          particles[i].transform.translation =
              Add(particles[i].transform.translation, addPos);
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Reset Offset")) {
        addPos = {0.0f, 0.0f, 0.0f};
      }

      ImGui::Separator();

      if (ImGui::Button("Reset All Transform/Velocity")) {
        for (int i = 0; i < instanceCount; ++i) {
          particles[i].transform.translation = {0.0f, 0.0f, 0.0f};
          particles[i].transform.rotation = {0.0f, 0.0f, 0.0f};
          particles[i].transform.scale = {1.0f, 1.0f, 1.0f};
          particles[i].velocity = {0.0f, 0.0f, 0.0f};
        }
      }

      ImGui::TreePop();
    }

    // ================
    // UV Transform一括
    // ================
    if (ImGui::TreeNode("UV Transform")) {
      bool uvChanged = false;

      uvChanged |=
          ImGui::DragFloat2("UV Scale", &uvScale_.x, 0.01f, 0.0f, 10.0f);
      uvChanged |= ImGui::DragFloat2("UV Translate", &uvTranslate_.x, 0.01f);
      uvChanged |= ImGui::DragFloat("UV Rotate", &uvRotate_, 0.01f);

      if (uvChanged && cbMat_) {
        // マテリアルのUV行列を更新（全パーティクル共通）
        SpriteMaterial *mapped = nullptr;
        if (SUCCEEDED(
                cbMat_->Map(0, nullptr, reinterpret_cast<void **>(&mapped)))) {
          RC::Vector3 s{uvScale_.x, uvScale_.y, 1.0f};
          RC::Vector3 r{0.0f, 0.0f, uvRotate_};
          RC::Vector3 t{uvTranslate_.x, uvTranslate_.y, 0.0f};
          mapped->uvTransform = MakeAffineMatrix(s, r, t);
          cbMat_->Unmap(0, nullptr);
        }
      }

      ImGui::TreePop();
    }

    // ====================
    // 各パーティクル個別編集
    // ====================
    if (ImGui::TreeNode("Particles")) {
      for (int i = 0; i < instanceCount; ++i) {
        auto &p = particles[i];

        char label[32];
        std::snprintf(label, sizeof(label), "Particle %d", i);

        if (ImGui::TreeNode(label)) {
          ImGui::DragFloat3("Translation", &p.transform.translation.x, 0.01f);
          ImGui::DragFloat3("Rotation", &p.transform.rotation.x, 0.01f);
          ImGui::DragFloat3("Scale", &p.transform.scale.x, 0.01f);
          ImGui::DragFloat3("Velocity", &p.velocity.x, 0.001f);

          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }

    ImGui::TreePop();
  }

#endif // _DEBUG
}

ParticleData Particle::MakeNewParticle(std::mt19937 &randomEngine) {

  std::uniform_real_distribution<float> distribution{-1.0f, 1.0f};
  ParticleData particle;
  particle.transform.scale = {1.0f, 1.0f, 1.0f};
  particle.transform.rotation = {0.0f, 0.0f, 0.0f};
  particle.transform.translation = {
      distribution(randomEngine),
      distribution(randomEngine),
      distribution(randomEngine),
  };
  particle.velocity = {distribution(randomEngine) * 0.01f,
                       distribution(randomEngine) * 0.01f,
                       distribution(randomEngine) * 0.01f};

  std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
  particle.color = {distColor(randomEngine), distColor(randomEngine),
                    distColor(randomEngine), 1.0f};

  return particle;
}

} // namespace RC
