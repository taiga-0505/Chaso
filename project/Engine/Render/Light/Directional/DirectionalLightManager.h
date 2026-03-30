#pragma once

#include "Light/Directional/DirectionalLightSource.h" // RC::DirectionalLightSource / DirectionalLight

#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

namespace RC {

/// <summary>
/// Light を「ハンドル制」で管理し、各ライト用の Upload CB（LightSlotCB）も保持する。
/// RenderCommon からライトの寿命/CB管理を分離するためのクラス。
/// </summary>
class DirectionalLightManager {
public:
  DirectionalLightManager() = default;
  ~DirectionalLightManager() { Term(); }

  DirectionalLightManager(const DirectionalLightManager &) = delete;
  DirectionalLightManager &operator=(const DirectionalLightManager &) = delete;

  /// <summary>初期化（Device が有効になった後に一度だけ呼ぶ）</summary>
  void Init(ID3D12Device *device);

  /// <summary>終了（確保したCBなどを解放）</summary>
  void Term();

  /// <summary>
  /// ライトを生成してハンドルを返す（0 はデフォルトスロット予約）
  /// </summary>
  /// <returns>ライトハンドル（失敗時 -1）</returns>
  int Create();

  /// <summary>ライトを破棄する（0 は破棄不可）</summary>
  void Destroy(int handle);

  /// <summary>
  /// 3D描画で使用する「アクティブライト」を切り替える。
  /// -1 の場合は「明示的なアクティブ無し」になり、内部では default(0) を使う。
  /// </summary>
  void SetActive(int handle);

  /// <summary>現在の「明示的なアクティブ」ハンドルを返す（未設定なら -1）</summary>
  int GetActiveHandle() const { return activeHandle_; }

  /// <summary>ライトの実体ポインタを取得（無効なら nullptr）</summary>
  DirectionalLightSource *Get(int handle);

  /// <summary>ライトの実体ポインタを取得（無効なら nullptr）</summary>
  const DirectionalLightSource *Get(int handle) const;

  /// <summary>描画時に使用される「実効アクティブ」を返す（未設定時は default(0)）</summary>
  DirectionalLightSource *GetActive();

  /// <summary>描画時に使用される「実効アクティブ」を返す（未設定時は default(0)）</summary>
  const DirectionalLightSource *GetActive() const;

  /// <summary>実効アクティブの LightSlotCB の GPU アドレスを返す（準備できない時は 0）</summary>
  D3D12_GPU_VIRTUAL_ADDRESS GetActiveCBAddress();

  /// <summary>指定ハンドルの LightSlotCB の GPU アドレスを返す（準備できない時は 0）</summary>
  D3D12_GPU_VIRTUAL_ADDRESS GetCBAddress(int handle);

  /// <summary>ライトの ImGui 表示を行う（編集後は GPU バッファにも反映）</summary>
  void DrawImGui(int handle, const char *name);

  /// <summary>明示的なアクティブが設定されているか（>=0）を返す</summary>
  bool HasExplicitActive() const { return activeHandle_ >= 0; }

  /// <summary>デフォルトライトのハンドル（常に 0）</summary>
  int DefaultHandle() const { return 0; }

private:
  struct Slot {
    DirectionalLightSource light;

    // Upload CB for this light (b1).
    Microsoft::WRL::ComPtr<ID3D12Resource> cb;
    DirectionalLight *mapped = nullptr;

    bool inUse = false;
  };

  int AllocSlot_();
  bool IsValid_(int handle) const;
  int ResolveActiveHandle_() const;

  void ReleaseSlot_(Slot &s);
  void EnsureCB_(Slot &s);
  void SyncCB_(Slot &s);

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  std::vector<Slot> slots_;

  // -1 = 明示的なアクティブ無し（default slot を使う）
  int activeHandle_ = -1;

  bool initialized_ = false;
};

} // namespace RC
