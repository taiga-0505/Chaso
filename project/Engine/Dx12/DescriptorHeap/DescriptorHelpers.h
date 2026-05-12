#pragma once
#include "DescriptorHeap.h"
#include <d3d12.h>

/// @file DescriptorHelpers.h
/// @brief デスクリプタの作成を簡略化するヘルパー関数群

/// @brief 2Dレンダータゲットビュー (RTV) を作成する
/// @param device DirectX12デバイス
/// @param rtvHeap RTVを配置するデスクリプタヒープ
/// @param res 対象のリソース
/// @param fmt ビューのフォーマット
/// @return 作成されたデスクリプタのCPUハンドル
inline D3D12_CPU_DESCRIPTOR_HANDLE
CreateRTV2D(ID3D12Device *device, DescriptorHeap &rtvHeap, ID3D12Resource *res,
            DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
  auto h = rtvHeap.AllocateCPU();
  D3D12_RENDER_TARGET_VIEW_DESC desc{};
  desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  desc.Format = fmt;
  desc.Texture2D.MipSlice = 0;
  desc.Texture2D.PlaneSlice = 0;
  device->CreateRenderTargetView(res, &desc, h);
  return h;
}

/// @brief 2D深度ステンシルビュー (DSV) を作成する
/// @param device DirectX12デバイス
/// @param dsvHeap DSVを配置するデスクリプタヒープ
/// @param res 対象のリソース
/// @param fmt ビューのフォーマット
/// @return 作成されたデスクリプタのCPUハンドル
inline D3D12_CPU_DESCRIPTOR_HANDLE
CreateDSV2D(ID3D12Device *device, DescriptorHeap &dsvHeap, ID3D12Resource *res,
            DXGI_FORMAT fmt = DXGI_FORMAT_D24_UNORM_S8_UINT) {
  auto h = dsvHeap.AllocateCPU();
  D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
  desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  desc.Format = fmt;
  device->CreateDepthStencilView(res, &desc, h);
  return h;
}

/// @brief 2Dシェーダーリソースビュー (SRV) を作成する
/// @param device DirectX12デバイス
/// @param srvHeap SRVを配置するデスクリプタヒープ
/// @param res 対象のリソース
/// @param fmt ビューのフォーマット
/// @param mipLevels ミップマップレベル数
/// @param mostDetailedMip 最も詳細なミップレベル (デフォルト 0)
/// @return 作成されたデスクリプタのCPUハンドル
inline D3D12_CPU_DESCRIPTOR_HANDLE
CreateSRV2D(ID3D12Device *device, DescriptorHeap &srvHeap, ID3D12Resource *res,
            DXGI_FORMAT fmt, UINT mipLevels, UINT mostDetailedMip = 0) {
  auto h = srvHeap.AllocateCPU();
  D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
  desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  desc.Format = fmt; // 例: DirectX::MakeSRGB(fmt) で sRGB に寄せられる
  desc.Texture2D.MostDetailedMip = mostDetailedMip;
  desc.Texture2D.MipLevels = mipLevels;
  desc.Texture2D.ResourceMinLODClamp = 0.0f;
  device->CreateShaderResourceView(res, &desc, h);
  return h;
}

/// @brief 定数バッファビュー (CBV) を作成する
/// @param device DirectX12デバイス
/// @param srvHeap CBVを配置するデスクリプタヒープ (通常はCBV/SRV/UAVヒープ)
/// @param bufferLocation バッファのGPU仮想アドレス
/// @param sizeBytes バッファのサイズ（バイト）。内部で256バイト境界にアラインされます。
/// @return 作成されたデスクリプタのCPUハンドル
inline D3D12_CPU_DESCRIPTOR_HANDLE
CreateCBV(ID3D12Device *device, DescriptorHeap &srvHeap,
          D3D12_GPU_VIRTUAL_ADDRESS bufferLocation, UINT sizeBytes) {
  auto h = srvHeap.AllocateCPU();
  D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
  // 256バイトアライン
  desc.SizeInBytes = (sizeBytes + 255) & ~255u;
  desc.BufferLocation = bufferLocation;
  device->CreateConstantBufferView(&desc, h);
  return h;
}
