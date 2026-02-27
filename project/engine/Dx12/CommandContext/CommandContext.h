#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

class CommandContext {
public:
  static constexpr uint32_t kMaxFrames = 3;

  CommandContext() = default;
  ~CommandContext() { Term(); }

  void Init(ID3D12Device *device,
            D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            uint32_t frameCount = 2);

  void Term();

  // backBufferIndex を渡す
  void BeginFrame(uint32_t frameIndex);

  void EndFrame();

  void WaitForFrame(uint32_t frameIndex);

  void FlushGPU();

  // 便利: 単発トランジション
  void Transition(ID3D12Resource *res, D3D12_RESOURCE_STATES before,
                  D3D12_RESOURCE_STATES after);

  // アクセサ
  ID3D12CommandQueue *Queue() const { return queue_.Get(); }
  ID3D12GraphicsCommandList *List() const { return list_.Get(); }
  uint32_t FrameCount() const { return frameCount_; }

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT;

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
  std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kMaxFrames> alloc_{};
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;

  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  HANDLE fenceEvent_ = nullptr;
  uint64_t globalFenceValue_ = 0;
  std::array<uint64_t, kMaxFrames> frameFenceValue_{};

  uint32_t frameCount_ = 2;
  uint32_t currentFrameIndex_ = 0;
};
