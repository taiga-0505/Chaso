#pragma once
#include "Light/Spot/SpotLightSource.h"
#include "struct.h"
#include <array>
#include <d3d12.h>
#include <vector>

namespace RC {

/// <summary>
/// SpotLight を管理する（最大4つ同時に有効化）
/// - Create/Destroy でスロットを確保
/// - AddActive/RemoveActive で「同時に効かせるライト」を選ぶ
/// - GPUへ送るCBは 1個（SpotLightsCB / b4）
/// </summary>
class SpotLightManager {
public:
  static constexpr int kMaxActive = 4;

  void Init(ID3D12Device *device);
  void Term();

  int Create();
  void Destroy(int handle);

  // 互換：1個だけアクティブにする（内部的に Clear→Add）
  void SetActive(int handle);
  int GetActiveHandle() const { return (activeCount_ > 0) ? active_[0] : -1; }

  // 複数アクティブ
  void ClearActive();
  bool AddActive(int handle);
  void RemoveActive(int handle);

  int GetActiveCount() const { return activeCount_; }
  int GetActiveHandleAt(int index) const;

  SpotLightSource *Get(int handle);
  const SpotLightSource *Get(int handle) const;

  // 互換：先頭のアクティブを返す（無ければ default slot）
  SpotLightSource *GetActive();
  const SpotLightSource *GetActive() const;

  // b4 用CB（SpotLightsCB）の GPU アドレス
  D3D12_GPU_VIRTUAL_ADDRESS GetCBAddress();

  void DrawImGui(int handle, const char *name);

private:
  struct Slot {
    bool inUse = false;
    SpotLightSource light{};
  };

  bool IsValid_(int handle) const;
  int AllocSlot_();
  int ResolveFallbackHandle_() const;

  void EnsureCB_();
  void SyncCB_();

private:
  ID3D12Device *device_ = nullptr;
  bool initialized_ = false;

  std::vector<Slot> slots_;

  std::array<int, kMaxActive> active_{};
  int activeCount_ = 0;

  ID3D12Resource *cb_ = nullptr;
  ::SpotLightsCB *mapped_ = nullptr;
};

} // namespace RC
