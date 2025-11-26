#include "Particle.h"
#include <Math/Math.h>
#include <function/function.h>
#include "RenderCommon.h"

namespace RC {

void Particle::Initialize(ID3D12Device *device) {

  device_ = device;

  modelData.vertices.clear();
  //modelData.vertices.reserve(6);

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

  // 2) 頂点バッファを作る
  vbResource_ =
      CreateBufferResource(device_, sizeof(VertexData) * vertexCount_);
  VertexData *vbMapped = nullptr;
  vbResource_->Map(0, nullptr, reinterpret_cast<void **>(&vbMapped));
  std::memcpy(vbMapped, modelData.vertices.data(),
              sizeof(VertexData) * vertexCount_);
  vbResource_->Unmap(0, nullptr);

  vbView_.BufferLocation = vbResource_->GetGPUVirtualAddress();
  vbView_.SizeInBytes = sizeof(VertexData) * vertexCount_;
  vbView_.StrideInBytes = sizeof(VertexData);

  // 3) 粒ごとの WVP 用 CB（instanceCount 分まとめて）
  cbStride_ = Align256(sizeof(TransformationMatrix));
  const uint32_t totalSize = cbStride_ * instanceCount;
  cbWvp_ = CreateBufferResource(device_, totalSize);
  cbWvp_->Map(0, nullptr, reinterpret_cast<void **>(&cbWvpMapped_));

  // 4) Material CB（1個だけ）
  cbMat_ = CreateBufferResource(device_, Align256(sizeof(Material)));
  cbMat_->Map(0, nullptr, reinterpret_cast<void **>(&cbMatMapped_));
  *cbMatMapped_ = Material{}; // 一旦 0 クリア
  cbMatMapped_->color = {1, 1, 1, 1};
  cbMatMapped_->uvTransform = MakeIdentity4x4();
  cbMatMapped_->lightingMode = (int)LightingMode::None;
  // 毎フレ更新しないなら閉じてしまってOK
  cbMat_->Unmap(0, nullptr);
  cbMatMapped_ = nullptr;

  // 5) Light CB（とりあえず白い平行光）
  cbLight_ = CreateBufferResource(device_, Align256(sizeof(DirectionalLight)));
  cbLight_->Map(0, nullptr, reinterpret_cast<void **>(&cbLightMapped_));
  *cbLightMapped_ = DirectionalLight{};
  cbLightMapped_->color = {1, 1, 1};
  cbLightMapped_->intensity = 1.0f;
  cbLight_->Unmap(0, nullptr);
  cbLightMapped_ = nullptr;

  // 6) テクスチャ（uvChecker）
  int texHandle = RC::LoadTex("Resources/uvChecker.png", true);
  textureSrv_ = RC::GetSrv(texHandle);
}

void Particle::Finalize() {
  if (cbWvp_) {
    cbWvp_->Unmap(0, nullptr);
    cbWvpMapped_ = nullptr;
    cbWvp_->Release();
    cbWvp_ = nullptr;
  }

  if (vbResource_) {
    vbResource_->Release();
    vbResource_ = nullptr;
  }

  // ★ここは Unmap しない（Initialize で一回だけ Unmap 済み）
  if (cbMat_) {
    cbMat_->Release();
    cbMat_ = nullptr;
  }

  if (cbLight_) {
    cbLight_->Release();
    cbLight_ = nullptr;
  }

  device_ = nullptr;
}

void Particle::Update(const Matrix4x4 &view, const Matrix4x4 &proj) {
  if (!cbWvpMapped_) {
    return;
  }

  for (int i = 0; i < instanceCount; ++i) {
    // とりあえず X 方向に等間隔で並べる
    Transform t{};
    t.scale = {1.0f, 1.0f, 1.0f};
    t.rotation = {0.0f, 0.0f, 0.0f};
    t.translation = {
        (i - instanceCount / 2) * 0.5f, // X だけずらす
        (i - instanceCount / 2) * 0.1f,
        0.0f,
    };

    Matrix4x4 world = MakeAffineMatrix(t.scale, t.rotation, t.translation);
    TransformationMatrix wvp{};
    wvp.World = world;
    wvp.WVP = Multiply(world, Multiply(view, proj));

    std::memcpy(cbWvpMapped_ + cbStride_ * i, &wvp, sizeof(wvp));
  }
}

void Particle::Render(ID3D12GraphicsCommandList *cl) {
  if (!vbResource_ || !cbWvp_ || vertexCount_ == 0) {
    return;
  }

  // IA 設定
  cl->IASetVertexBuffers(0, 1, &vbView_);
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Material / Texture / Light は粒共通で1回だけセット
  if (cbMat_) {
    cl->SetGraphicsRootConstantBufferView(0, cbMat_->GetGPUVirtualAddress());
  }
  if (textureSrv_.ptr != 0) {
    cl->SetGraphicsRootDescriptorTable(2, textureSrv_);
  }
  if (cbLight_) {
    cl->SetGraphicsRootConstantBufferView(3, cbLight_->GetGPUVirtualAddress());
  }

  // 粒ごとに WVP のアドレスだけ差し替えて DrawInstanced
  for (int i = 0; i < instanceCount; ++i) {
    D3D12_GPU_VIRTUAL_ADDRESS addr =
        cbWvp_->GetGPUVirtualAddress() + cbStride_ * i;
    cl->SetGraphicsRootConstantBufferView(1, addr);
    cl->DrawInstanced(vertexCount_, 1, 0, 0);
  }
}



} // namespace RC
