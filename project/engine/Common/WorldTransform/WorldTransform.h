#pragma once
#include "RC.h"
#include <d3d12.h>
#include <type_traits>
#include <wrl.h>

namespace RC {

// 定数バッファ用データ構造体
struct ConstBufferDataWorldTransform {
  Matrix4x4 matWorld; // ローカル → ワールド変換行列
};

class WorldTransform {
public:
  // ローカルスケール
  RC::Vector3 scale = {1.0f, 1.0f, 1.0f};
  // ローカル回転（オイラー角）
  RC::Vector3 rotation = {0.0f, 0.0f, 0.0f};
  // ローカル座標
  RC::Vector3 translation = {0.0f, 0.0f, 0.0f};

  // ローカル → ワールド変換行列
  RC::Matrix4x4 matWorld;
  // 親となるワールド変換へのポインタ
  const WorldTransform *parent_ = nullptr;

  WorldTransform() = default;
  ~WorldTransform() = default;

  // 初期化（CB作成・Map・初回転送）
  void Initialize();

  // ワールド行列の更新（計算だけ）
  void UpdateMatrix();

  // 定数バッファ生成
  void CreateConstBuffer();

  // マッピングする
  void Map();

  // 行列を転送する
  void TransferMatrix();

  const Microsoft::WRL::ComPtr<ID3D12Resource> &GetConstBuffer() const {
    return constBuffer_;
  }

  // ワールド座標を取得
  Vector3 GetWorldPosition() const {
    return {matWorld.m[3][0], matWorld.m[3][1], matWorld.m[3][2]};
  }

private:
  // 定数バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
  // マッピング済みアドレス
  ConstBufferDataWorldTransform *constMap = nullptr;

  // コピー禁止
  WorldTransform(const WorldTransform &) = delete;
  WorldTransform &operator=(const WorldTransform &) = delete;
};

static_assert(!std::is_copy_assignable_v<WorldTransform>);

} // namespace RC
