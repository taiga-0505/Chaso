#include "Primitive3D.h"
#include <cassert>
#include <cmath>

Primitive3D::~Primitive3D() { Term(); }

void Primitive3D::Term() {
  if (vb_) {
    if (vbMap_)
      vb_->Unmap(0, nullptr);
    vb_->Release();
    vb_ = nullptr;
    vbMap_ = nullptr;
    vbCapacity_ = 0;
  }
  if (cbRes_) {
    if (cbMap_)
      cbRes_->Unmap(0, nullptr);
    cbRes_->Release();
    cbRes_ = nullptr;
    cbMap_ = nullptr;
  }
  device_ = nullptr;
  Clear();
}

void Primitive3D::Initialize(ID3D12Device *device) {
  Term();
  device_ = device;

  cbStride_ = Align256((uint32_t)sizeof(PerFrameCB));
  cbRes_ = CreateBufferResource(device_, cbStride_ * kMaxDrawPerFrame);
  cbRes_->Map(0, nullptr, reinterpret_cast<void **>(&cbMap_));

  // 初期VB（とりあえず 4096 頂点）
  EnsureVB_(4096);
}

void Primitive3D::BeginFrame(const RC::Matrix4x4 &view,
                             const RC::Matrix4x4 &proj, BlendMode blendAt3D) {
  perFrame_.View = view;
  perFrame_.Proj = proj;
  blendAt3D_ = blendAt3D;

  cbCursor_ = 0;
  Clear();
}

void Primitive3D::Clear() {
  vtxDepth_.clear();
  vtxNoDepth_.clear();
}

void Primitive3D::EnsureVB_(size_t vertexCount) {
  if (vb_ && vbCapacity_ >= vertexCount)
    return;

  // 既存解放
  if (vb_) {
    if (vbMap_)
      vb_->Unmap(0, nullptr);
    vb_->Release();
    vb_ = nullptr;
    vbMap_ = nullptr;
    vbCapacity_ = 0;
  }

  // ちょい余裕持たせる（伸びた時の再確保回数を減らす）
  vbCapacity_ = (vertexCount < 4096) ? 4096 : vertexCount;
  vb_ = CreateBufferResource(device_, sizeof(Vertex) * vbCapacity_);
  vb_->Map(0, nullptr, reinterpret_cast<void **>(&vbMap_));

  vbView_.BufferLocation = vb_->GetGPUVirtualAddress();
  vbView_.StrideInBytes = (UINT)sizeof(Vertex);
  // SizeInBytes は Draw の直前に “実際の頂点数” で上書きする
}

void Primitive3D::AddLine(const RC::Vector3 &a, const RC::Vector3 &b,
                          const RC::Vector4 &color, bool depth) {
  auto &v = depth ? vtxDepth_ : vtxNoDepth_;
  v.push_back({a, color});
  v.push_back({b, color});
}

void Primitive3D::AddAABB(const RC::Vector3 &mn, const RC::Vector3 &mx,
                          const RC::Vector4 &c, bool depth) {
  RC::Vector3 p[8] = {
      {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z},
      {mn.x, mx.y, mn.z}, {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
      {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
  };
  const int e[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                     6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
  for (int i = 0; i < 24; i += 2) {
    AddLine(p[e[i]], p[e[i + 1]], c, depth);
  }
}

void Primitive3D::AddGridXZ(int halfSize, float step, const RC::Vector4 &c,
                            bool depth) {
  const float max = halfSize * step;
  for (int i = -halfSize; i <= halfSize; ++i) {
    float x = i * step;
    AddLine({x, 0, -max}, {x, 0, +max}, c, depth);
    float z = i * step;
    AddLine({-max, 0, z}, {+max, 0, z}, c, depth);
  }
}

void Primitive3D::AddGridXY(int halfSize, float step, const RC::Vector4 &color,
                            bool depth) {
  const float max = halfSize * step;
  for (int i = -halfSize; i <= halfSize; ++i) {
    float x = i * step;
    AddLine({x, -max, 0}, {x, +max, 0}, color, depth);
    float y = i * step;
    AddLine({-max, y, 0}, {+max, y, 0}, color, depth);
  }
}

void Primitive3D::AddGridYZ(int halfSize, float step, const RC::Vector4 &color,
                            bool depth) {
  const float max = halfSize * step;
  for (int i = -halfSize; i <= halfSize; ++i) {
    float y = i * step;
    AddLine({0, y, -max}, {0, y, +max}, color, depth);
    float z = i * step;
    AddLine({0, -max, z}, {0, +max, z}, color, depth);
  }
}

void Primitive3D::Draw(ID3D12GraphicsCommandList *cl, bool depth) {
  auto &v = depth ? vtxDepth_ : vtxNoDepth_;
  if (v.empty() || !cl || !device_)
    return;

  EnsureVB_(v.size());

  // VB 転送（Upload heap を Map しっぱなし運用）
  std::memcpy(vbMap_, v.data(), sizeof(Vertex) * v.size());
  vbView_.SizeInBytes = (UINT)(sizeof(Vertex) * v.size());

  // CB ring（Drawごとに別スロット）
  const uint32_t idx =
      (cbCursor_ < kMaxDrawPerFrame) ? cbCursor_++ : (cbCursor_ = 1, 0);

  std::memcpy(cbMap_ + idx * cbStride_, &perFrame_, sizeof(PerFrameCB));
  const D3D12_GPU_VIRTUAL_ADDRESS cbAddr =
      cbRes_->GetGPUVirtualAddress() + idx * cbStride_;

  cl->IASetVertexBuffers(0, 1, &vbView_);
  cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

  // RootParam[0] = b0(VS)
  cl->SetGraphicsRootConstantBufferView(0, cbAddr);

  cl->DrawInstanced((UINT)v.size(), 1, 0, 0);

  // 描いた分は消してOK（“そのフレームだけ”）
  v.clear();
}
