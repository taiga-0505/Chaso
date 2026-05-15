#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

/// @brief スクリーンショット撮影機能を提供するユーティリティクラス
class ScreenCapture {
public:
    /// @brief バックバッファをキャプチャしてファイルに保存する
    /// @param device D3D12デバイス
    /// @param queue コマンドキュー（GPUとの同期用）
    /// @param backBuffer キャプチャ対象のバックバッファリソース
    /// @return 保存されたファイルのパス（失敗した場合は空文字列）
    static std::string SaveScreenshot(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Resource* backBuffer);

    /// @brief 通知UIの表示時間（秒）
    static constexpr float kNotifyDisplayTime = 3.0f;

    /// @brief 通知UIの描画
    static void DrawImGui(float deltaTime, class Dx12Core* core);

private:
    struct NotifyData {
        std::string path;
        int texHandle = -1;
        float timer = 0.0f;
        bool active = false;
        int pendingUnloadHandle = -1;
    };
    static NotifyData notify_;

    /// @brief スクリーンショット保存用のフォルダを作成する
    static void CreateScreenshotDirectory();
    
    /// @brief 現在の時刻からファイル名を生成する
    static std::string GenerateTimestampFileName();
};
