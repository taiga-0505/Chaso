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
    /// @return 成功すれば true
    static bool SaveScreenshot(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Resource* backBuffer);

private:
    /// @brief スクリーンショット保存用のフォルダを作成する
    static void CreateScreenshotDirectory();
    
    /// @brief 現在の時刻からファイル名を生成する
    static std::string GenerateTimestampFileName();
};
