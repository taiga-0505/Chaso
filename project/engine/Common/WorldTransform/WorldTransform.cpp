#include "WorldTransform.h"
#include "Math/Math.h"    // ← MakeAffineMatrix / Multiply
#include "RenderCommon.h" // ← GetDevice() 使う

namespace RC {

void WorldTransform::Initialize() {
  CreateConstBuffer();
  Map();
  TransferMatrix();
}

void WorldTransform::CreateConstBuffer() {
  ID3D12Device *device = RC::GetDevice();
  if (!device) {
    // RenderCommon::Init がまだ or 初期化失敗
    return;
  }

  // CBは256byteアライン必須
  const UINT sizeInBytes =
      (static_cast<UINT>(sizeof(ConstBufferDataWorldTransform)) + 0xFFu) &
      ~0xFFu;

  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC resDesc{};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = sizeInBytes;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                  IID_PPV_ARGS(&constBuffer_));
}

void WorldTransform::Map() {
  if (!constBuffer_ || constMap) {
    return;
  }
  constBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&constMap));
}

void WorldTransform::UpdateMatrix() {

  Matrix4x4 local = MakeAffineMatrix(scale, rotation, translation);

  if (parent_) {
    matWorld = Multiply(local, parent_->matWorld);
  } else {
    matWorld = local;
  }
}

void WorldTransform::TransferMatrix() {
  UpdateMatrix();

  if (!constMap) {
    Map();
  }
  if (!constMap) {
    return;
  }
  constMap->matWorld = matWorld;
}

} // namespace RC
