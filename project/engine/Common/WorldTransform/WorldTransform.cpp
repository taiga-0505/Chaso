#include "WorldTransform.h"
#include "Math.h" // MakeAffineMatrix / Multiply を使う

using namespace RC;

namespace RC {
extern ID3D12Device *gDevice;

void WorldTransform::Initialize() {
  CreateConstBuffer();
  Map();
  TransferMatrix(); // 初期値（scale=1, rot=0, trans=0）でも一回転送
}

void WorldTransform::CreateConstBuffer() {
  // D3D12のCBは 256byte アライン必須
  const UINT sizeInBytes =
      (static_cast<UINT>(sizeof(ConstBufferDataWorldTransform)) + 0xFFu) &
      ~0xFFu;

  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_DESC resDesc{};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = sizeInBytes;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  // 作成
  HRESULT hr = gDevice->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constBuffer_));
  (void)hr; // 必要ならassert(hr==S_OK)とかにしてOK
}

void WorldTransform::Map() {
  if (!constBuffer_) {
    return;
  }
  if (constMap) {
    return; // すでにMap済み
  }

  HRESULT hr =
      constBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&constMap));
  (void)hr;
}

void WorldTransform::UpdateMatrix() {
  // ローカル行列（S * R * T）
  Matrix4x4 local = MakeAffineMatrix(scale, rotation, translation);

  // 親がいるなら：local * parent（row-vector規約の合成）
  if (parent_) {
    matWorld = Multiply(local, parent_->matWorld);
  } else {
    matWorld = local;
  }
}

void WorldTransform::TransferMatrix() {
  // まず行列計算
  UpdateMatrix();

  // MapされてなければMap
  if (!constMap) {
    Map();
  }
  if (!constMap) {
    return;
  }

  // CBへコピー
  constMap->matWorld = matWorld;
}

} // namespace RC
