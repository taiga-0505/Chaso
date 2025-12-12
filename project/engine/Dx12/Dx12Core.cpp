#include "Dx12Core.h"
#include <cassert>
#include <dxgidebug.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;
// ---------- Debug helpers ----------
static void ReportD3D12LiveObjects(ID3D12Device *device) {
  if (!device)
    return;
  ComPtr<ID3D12DebugDevice> dbg;
  if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dbg)))) {
    // DETAIL を使うと親子関係まで見える
    dbg->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
  }
}
static void ReportDXGILiveObjects() {
  ComPtr<IDXGIDebug1> dxgiDebug;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
    // ALL で全カテゴリ。必要なら DXGI_DEBUG_D3D12 だけに絞ってもOK
    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
  }
}

void Dx12Core::Init(HWND hwnd, const Desc &d) {
  desc_ = d;

  // Device
  device_.Init(d.debug, d.gpuValidation);
  device_.SetupInfoQueue(true, true);
  allowTearing_ = d.allowTearingIfSupported && device_.IsTearingSupported();

  ID3D12Device *dev = device_.GetDevice();

  // Command
  cmd_.Init(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, d.frameCount);

  // Heaps
  rtv_.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, d.frameCount, false);
  dsv_.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
  srv_.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d.srvHeapCapacity,
            true);

  sbMgr_.Init(GetDevice(), &srv_); 

  // SwapChain
  swap_.SetRtvHeap(rtv_.Heap(), rtv_.Increment());
  // スワップチェーンは UNORM で作成（RTVはSRGBビューで作る）
  DXGI_FORMAT scFormat = (desc_.rtvFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                             ? DXGI_FORMAT_R8G8B8A8_UNORM
                             : desc_.rtvFormat; // それ以外はそのまま
  swap_.Init(device_.Factory(), dev, cmd_.Queue(), hwnd, d.width, d.height,
             scFormat, d.frameCount, allowTearing_);

  // Depth
  depth_.Init(dev, d.width, d.height, dsv_, d.dsvFormat, d.dsvFormat);

  ResetViewportScissorToBackbuffer(d.width, d.height);

  // FPS固定の初期化（VSyncは切らずに追加待ちを入れる運用）
  if (fixFpsEnabled_) {
    fixFps_ = std::make_unique<FixFps>();
    fixFps_->Initialize();
  }

  // Pipeline（頂点レイアウトは最小例。必要なら外から差し替え可）
  D3D12_INPUT_ELEMENT_DESC inputElems[3] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
  };
}

void Dx12Core::BeginFrame() {
  backIndex_ = swap_.CurrentBackBufferIndex();
  cmd_.BeginFrame(backIndex_);

  // Present → RenderTarget
  cmd_.Transition(swap_.BackBuffer(backIndex_), D3D12_RESOURCE_STATE_PRESENT,
                  D3D12_RESOURCE_STATE_RENDER_TARGET);

  // OM バインド
  auto *cl = cmd_.List();
  auto rtv = swap_.RtvAt(backIndex_);
  auto dsv = depth_.Dsv();
  cl->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

  // SRV heap セット（ImGui 等を先頭に置いている前提）
  ID3D12DescriptorHeap *heaps[] = {srv_.Heap()};
  cl->SetDescriptorHeaps(1, heaps);

  // ビューポート、シザー矩形セット
  cl->RSSetViewports(1, &viewport_);
  cl->RSSetScissorRects(1, &scissor_);

  Clear();
}

void Dx12Core::Clear(float r, float g, float b, float a) {
  float clear[4] = {r, g, b, a};
  cmd_.List()->ClearRenderTargetView(CurrentRTV(), clear, 0, nullptr);
  cmd_.List()->ClearDepthStencilView(
      Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
      nullptr);
}

void Dx12Core::EndFrame() {
  // RenderTarget → Present
  cmd_.Transition(swap_.BackBuffer(backIndex_),
                  D3D12_RESOURCE_STATE_RENDER_TARGET,
                  D3D12_RESOURCE_STATE_PRESENT);

  cmd_.EndFrame();
  // vsync=1, tearingなら 0 でもOK（好みで）
  swap_.Present(1, 0);
  cmd_.WaitForFrame(backIndex_);

  // ← VSync & フェンス待ち直後でFPS固定を実行
  if (fixFps_) {
    fixFps_->Update();
  }
}

void Dx12Core::WaitForGPU() { cmd_.FlushGPU(); }

void Dx12Core::Term() {
  // 1) GPU 完全停止
  cmd_.FlushGPU();
  // 2) フレーム依存のリソースから順に解放
  //    - BackBuffer を握るのは swap_ なので、先に Depth → SwapChain
  //    の順で。
  //    - RTV/DSV/SRV ヒープは BackBuffer/Depth が落ちた後に。
  depth_.Term(); // DSV リソース
  swap_.Term();  // BackBuffer リソース + SwapChain
  sbMgr_.Term();
  rtv_.Term();
  dsv_.Term();
  srv_.Term();
  // 3) コマンド系（Allocator/List/Queue/Fence 等）
  cmd_.Term();
  // 4) Live Objects レポート（デバイス・ファクトリが生きているうちに）
  ReportD3D12LiveObjects(device_.GetDevice());
  ReportDXGILiveObjects();
  // 5) 最後にデバイス/ファクトリ
  device_.Term();
}

void Dx12Core::EnableFixFps(bool enable) {
  fixFpsEnabled_ = enable;
  if (enable) {
    if (!fixFps_) {
      fixFps_ = std::make_unique<FixFps>();
      fixFps_->Initialize();
    }
  } else {
    fixFps_.reset();
  }
}
