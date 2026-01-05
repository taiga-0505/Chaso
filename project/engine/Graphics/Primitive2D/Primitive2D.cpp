#include "Primitive2D.h"
#include "RenderCommon.h" // RC::LoadTex / RC::GetSrv
#include <cassert>
#include <cstring>

namespace {
int sWhiteTexHandle = -1;

D3D12_GPU_DESCRIPTOR_HANDLE GetWhiteSrv_() {
  if (sWhiteTexHandle < 0) {
    sWhiteTexHandle = RC::LoadTex("Resources/white1x1.png");
    // LoadTexはRC::Init前だと -1 を返す :contentReference[oaicite:2]{index=2}
  }
  return RC::GetSrv(sWhiteTexHandle); // 失敗時は {0} が返る
                                      // :contentReference[oaicite:3]{index=3}
}
} // namespace

using namespace RC;

static void WriteFullTri(ID3D12Resource *vb) {
  Primitive2D::Vertex *v = nullptr;
  vb->Map(0, nullptr, reinterpret_cast<void **>(&v));

  // フルスクリーン三角形（clip座標）
  v[0] = {{-1, -1, 0, 1}, {0, 1}};
  v[1] = {{-1, 3, 0, 1}, {0, -1}};
  v[2] = {{3, -1, 0, 1}, {2, 1}};

  vb->Unmap(0, nullptr);
}

Primitive2D::~Primitive2D() { Release(); }

void Primitive2D::Release() {
  if (vb_.res) {
    vb_.res->Release();
    vb_.res = nullptr;
  }
  if (cbParamsRes_) {
    cbParamsRes_->Release();
    cbParamsRes_ = nullptr;
    cbParamsMap_ = nullptr;
  }
  if (cbParams_.res) {
    cbParams_.res->Release();
    cbParams_.res = nullptr;
    cbParams_.map = nullptr;
  }
  if (cbDummy_.res) {
    cbDummy_.res->Release();
    cbDummy_.res = nullptr;
    cbDummy_.map = nullptr;
  }
}

void Primitive2D::Initialize(ID3D12Device *device, float screenW,
                             float screenH) {
  Release();
  device_ = device;
  screenW_ = screenW;
  screenH_ = screenH;

  cbStride_ = Align256((uint32_t)sizeof(Params));

  // ★リング分まとめて確保
  cbParamsRes_ = CreateBufferResource(device_, cbStride_ * kMaxDrawPerFrame);
  cbParamsRes_->Map(0, nullptr, reinterpret_cast<void **>(&cbParamsMap_));

  // CPU側初期化
  paramsCPU_ = {};
  paramsCPU_.C = {screenW_, screenH_, 1.0f, 1.0f};
  paramsCPU_.Color = {1, 1, 1, 1};
  paramsCPU_.UVRect = {0, 0, 1, 1};
  paramsCPU_.U = {SHAPE_LINE, 0, 0, 0};

  // Dummy VS は今まで通り
  cbDummy_.res = CreateBufferResource(device_, sizeof(DummyVS));
  cbDummy_.res->Map(0, nullptr, reinterpret_cast<void **>(&cbDummy_.map));
  cbDummy_.map->World = MakeIdentity4x4();
  cbDummy_.map->WVP = MakeIdentity4x4();

  // VB は今まで通り
  vb_.res = CreateBufferResource(device_, sizeof(Vertex) * 3);
  WriteFullTri(vb_.res);
  vb_.view.BufferLocation = vb_.res->GetGPUVirtualAddress();
  vb_.view.SizeInBytes = sizeof(Vertex) * 3;
  vb_.view.StrideInBytes = sizeof(Vertex);

  if (srv_.ptr == 0) {
    srv_ = GetWhiteSrv_();
  }
}

void Primitive2D::SetScreenSize(float w, float h) {
  screenW_ = w;
  screenH_ = h;
  paramsCPU_.C.x = w;
  paramsCPU_.C.y = h;
}

void Primitive2D::SetLine(const Vector2 &p0, const Vector2 &p1, float thickness,
                          const Vector4 &color, float feather) {
  paramsCPU_.U = {SHAPE_LINE, 0, 0, 0};
  paramsCPU_.A = {p0.x, p0.y, p1.x, p1.y};
  paramsCPU_.C.z = thickness;
  paramsCPU_.C.w = feather;
  paramsCPU_.Color = color;
}

void Primitive2D::SetRect(const RC::Vector2 &mn, const RC::Vector2 &mx,
                          kFillMode fillMode, float thickness,
                          const RC::Vector4 &color, float feather) {
  const bool stroke = (fillMode == kWire);
  paramsCPU_.U = {SHAPE_RECT, stroke ? FLAG_STROKE : 0u, 0, 0};
  paramsCPU_.B = {mn.x, mn.y, mx.x, mx.y};
  paramsCPU_.C.z = thickness;
  paramsCPU_.C.w = feather;
  paramsCPU_.Color = color;
}

void Primitive2D::SetCircle(const RC::Vector2 &c, float r,
                            kFillMode fillMode, float thickness,
                            const RC::Vector4 &color, float feather) {
  const bool stroke = (fillMode == kWire);
  paramsCPU_.U = {SHAPE_CIRCLE, stroke ? FLAG_STROKE : 0u, 0, 0};
  paramsCPU_.D = {c.x, c.y, r, 0.0f};
  paramsCPU_.C.z = thickness;
  paramsCPU_.C.w = feather;
  paramsCPU_.Color = color;
}

void Primitive2D::SetTriangle(const RC::Vector2 &p0, const RC::Vector2 &p1,
                              const RC::Vector2 &p2, kFillMode fillMode,
                              float thickness, const RC::Vector4 &color,
                              float feather) {
  const bool stroke = (fillMode == kWire);
  paramsCPU_.U = {SHAPE_TRIANGLE, stroke ? FLAG_STROKE : 0u, 0, 0};
  paramsCPU_.A = {p0.x, p0.y, p1.x, p1.y};
  // B.xy に p2 を入れる（B.zw は未使用）
  paramsCPU_.B = {p2.x, p2.y, 0.0f, 0.0f};
  paramsCPU_.C.z = thickness;
  paramsCPU_.C.w = feather;
  paramsCPU_.Color = color;
}

void Primitive2D::SetSpriteRect(const Vector2 &mn, const Vector2 &mx,
                                const Vector4 &color, const Vector2 &uvMin,
                                const Vector2 &uvMax) {
  paramsCPU_.U = {SHAPE_SPRITE, FLAG_TEX, 0, 0};
  paramsCPU_.B = {mn.x, mn.y, mx.x, mx.y}; // PS側で使う想定
  paramsCPU_.UVRect = {uvMin.x, uvMin.y, uvMax.x, uvMax.y};
  paramsCPU_.Color = color;
}

void Primitive2D::BeginFrame() { cbCursor_ = 0; }

void Primitive2D::Draw(ID3D12GraphicsCommandList *cmdList) {
  if (!visible_)
    return;

  const uint32_t idx =
      (cbCursor_ < kMaxDrawPerFrame) ? cbCursor_++ : (cbCursor_ = 1, 0);

  // ★Drawごとに別スロットへ書く
  std::memcpy(cbParamsMap_ + idx * cbStride_, &paramsCPU_, sizeof(Params));

  const D3D12_GPU_VIRTUAL_ADDRESS gpuAddr =
      cbParamsRes_->GetGPUVirtualAddress() + idx * cbStride_;

  cmdList->IASetVertexBuffers(0, 1, &vb_.view);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  cmdList->SetGraphicsRootConstantBufferView(0, gpuAddr);
  cmdList->SetGraphicsRootConstantBufferView(
      1, cbDummy_.res->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootDescriptorTable(2, srv_);

  cmdList->DrawInstanced(3, 1, 0, 0);
}
