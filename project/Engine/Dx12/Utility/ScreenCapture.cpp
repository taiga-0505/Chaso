#include "ScreenCapture.h"
#include "DirectXTex/DirectXTex.h"
#include <filesystem>
#include <format>
#include <chrono>
#include "Common/Log/Log.h"

using namespace Microsoft::WRL;

bool ScreenCapture::SaveScreenshot(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Resource* backBuffer) {
    if (!device || !queue || !backBuffer) return false;

    // 1. リソース情報の取得
    D3D12_RESOURCE_DESC desc = backBuffer->GetDesc();
    
    // 2. Readback用リソースの作成に必要なサイズ計算
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT numRows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    ComPtr<ID3D12Resource> readbackResource;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = totalBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readbackResource));

    if (FAILED(hr)) {
        Log::Print("[ScreenCapture] Failed to create readback resource.");
        return false;
    }

    // 3. コピー用コマンドの作成と実行
    ComPtr<ID3D12CommandAllocator> allocator;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    ComPtr<ID3D12GraphicsCommandList> cl;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cl));

    // バリア: PRESENT (または現在の状態) -> COPY_SOURCE
    // ※ 呼び出し元が EndFrame の直後であれば PRESENT 状態のはず
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &barrier);

    // コピー実行
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = readbackResource.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = backBuffer;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;

    cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // バリア戻し: COPY_SOURCE -> PRESENT
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cl->ResourceBarrier(1, &barrier);

    cl->Close();
    ID3D12CommandList* lists[] = { cl.Get() };
    queue->ExecuteCommandLists(1, lists);

    // 4. GPUの完了を待機
    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    queue->Signal(fence.Get(), 1);
    HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fence->GetCompletedValue() < 1) {
        fence->SetEventOnCompletion(1, event);
        WaitForSingleObject(event, INFINITE);
    }
    CloseHandle(event);

    // 5. MapしてDirectXTexで保存
    void* mappedData = nullptr;
    hr = readbackResource->Map(0, nullptr, &mappedData);
    if (FAILED(hr)) {
        Log::Print("[ScreenCapture] Failed to map readback resource.");
        return false;
    }

    DirectX::Image image;
    image.width = static_cast<size_t>(desc.Width);
    image.height = static_cast<size_t>(desc.Height);
    image.format = desc.Format;
    image.rowPitch = static_cast<size_t>(footprint.Footprint.RowPitch);
    image.slicePitch = static_cast<size_t>(totalBytes);
    image.pixels = reinterpret_cast<uint8_t*>(mappedData);

    CreateScreenshotDirectory();
    std::string fileName = "screenshots/" + GenerateTimestampFileName() + ".png";
    
    // WIC保存用にフォーマット調整
    // バックバッファが UNORM でも、中身は sRGB (gamma) 空間のデータであるため、
    // SaveToWICFile に sRGB であることを伝えて二重ガンマ補正を防ぐ。
    if (image.format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        image.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    Log logger;
    std::wstring wFileName = logger.ConvertString(fileName);

    hr = DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), wFileName.c_str());
    
    readbackResource->Unmap(0, nullptr);

    if (FAILED(hr)) {
        Log::Print(std::format("[ScreenCapture] Failed to save screenshot. HRESULT={:08X}", (uint32_t)hr));
        return false;
    }

    Log::Print(std::format("[ScreenCapture] Screenshot saved: {}", fileName));
    return true;
}

void ScreenCapture::CreateScreenshotDirectory() {
    if (!std::filesystem::exists("screenshots")) {
        std::filesystem::create_directory("screenshots");
    }
}

std::string ScreenCapture::GenerateTimestampFileName() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time_t);
    return std::format("{:04}{:02}{:02}_{:02}{:02}{:02}", 
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
        tm.tm_hour, tm.tm_min, tm.tm_sec);
}
