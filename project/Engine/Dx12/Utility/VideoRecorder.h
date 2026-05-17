#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>

/// @brief Media Foundation を利用した画面録画クラス
class VideoRecorder {
public:
    VideoRecorder();
    ~VideoRecorder();

    /// @brief 録画を開始する
    /// @param device D3D12デバイス
    /// @param queue コマンドキュー
    /// @param width 解像度幅
    /// @param height 解像度高さ
    /// @param fps 録画フレームレート
    /// @param format バックバッファのフォーマット
    /// @return 成功したら true
    bool Start(ID3D12Device* device, ID3D12CommandQueue* queue, UINT width, UINT height, UINT fps, DXGI_FORMAT format);

    /// @brief 録画を停止する
    void Stop();

    /// @brief 毎フレームのキャプチャを更新・実行する
    /// @param backBuffer 現在のバックバッファ
    void Update(ID3D12Resource* backBuffer);

    /// @brief 現在録画中かどうか
    bool IsRecording() const { return isRecording_; }

    /// @brief 最新の保存パスを取得
    const std::string& GetLatestRecordPath() const { return latestPath_; }
    void ClearLatestRecordPath() { latestPath_.clear(); }

    /// @brief 通知UIの表示時間（秒）
    static constexpr float kNotifyDisplayTime = 3.0f;

    /// @brief ImGuiの描画（録画ボタンや通知UI等）
    void DrawImGui(float deltaTime, class Dx12Core* core);

private:
    struct NotifyData {
        std::string path;
        float timer = 0.0f;
        bool active = false;
    };
    NotifyData notify_;
    bool InitializeMF();
    void TerminateMF();
    bool SetupSinkWriter(const std::wstring& path, UINT width, UINT height, UINT fps);
    void FlushBuffers();

    bool isRecording_ = false;
    std::string latestPath_;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    
    // Media Foundation
    Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter_;
    DWORD streamIndex_ = 0;
    UINT64 rtStart_ = 0;
    UINT64 frameCount_ = 0;
    UINT videoFps_ = 60;
    UINT videoWidth_ = 0;
    UINT videoHeight_ = 0;
    DXGI_FORMAT videoFormat_ = DXGI_FORMAT_UNKNOWN;

    // Readback バッファ (ダブル/トリプルバッファリング用)
    static constexpr UINT kBufferCount = 3;
    struct ReadbackBuffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        HANDLE fenceEvent = nullptr;
        UINT64 fenceValue = 0;
        bool inUse = false;
    };
    std::vector<ReadbackBuffer> readbackBuffers_;
    UINT currentBufferIndex_ = 0;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    void ProcessReadbackBuffer(ReadbackBuffer& buffer);
    void WriteFrame(const void* data, UINT rowPitch);
};
