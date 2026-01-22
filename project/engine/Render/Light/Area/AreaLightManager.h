#pragma once
#include "Light/Area/AreaLightSource.h"
#include "struct.h"
#include <array>
#include <d3d12.h>
#include <vector>

namespace RC {

/// <summary>
/// Rect AreaLight を管理する（最大4つ同時に有効化）
/// - Create/Destroy でスロットを確保
/// - AddActive/RemoveActive で「同時に効かせるライト」を選ぶ
/// - GPUへ送るCBは 1個（AreaLightsCB / b5）
/// </summary>
class AreaLightManager {
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

  AreaLightSource *Get(int handle);
  const AreaLightSource *Get(int handle) const;

  // 互換：先頭のアクティブを返す（無ければ default slot）
  AreaLightSource *GetActive();
  const AreaLightSource *GetActive() const;

  // b5 用CB（AreaLightsCB）の GPU アドレス
  D3D12_GPU_VIRTUAL_ADDRESS GetCBAddress();

  void DrawImGui(int handle, const char *name);

private:
  struct Slot {
    bool inUse = false;
    AreaLightSource light{};
  };

  bool IsValid_(int handle) const;
  int AllocSlot_();
  int ResolveFallbackHandle_() const;

  void EnsureCB_();
  void SyncCB_(); // active list を詰めて mapped_ にコピー

private:
  ID3D12Device *device_ = nullptr;
  bool initialized_ = false;

  std::vector<Slot> slots_;

  // active handles (max 4)
  std::array<int, kMaxActive> active_{};
  int activeCount_ = 0;

  // GPU CB (single)
  ID3D12Resource *cb_ = nullptr;
  ::AreaLightsCB *mapped_ = nullptr;
};

} // namespace RC
