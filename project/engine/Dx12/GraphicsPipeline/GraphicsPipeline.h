#pragma once
#include <cassert>
#include <d3d12.h>
#include <vector>

enum BlendMode {
  // ブレンド無し
  kBlendModeNone,
  // 通常のαブレンド Src * SrcA + Dest * (1 - SrcA)
  kBlendModeNormal,
  // 加算 Src * SrcA + Dest * 1
  kBlendModeAdd,
  // 減算 Dest * 1 - Src * SrcA
  kBlendModeSubtract,
  // 乗算 Src * 0 + Dest * Src
  kBlendModeMultiply,
  // スクリーン Src * (1 - Dest) + Dest * 1
  kBlendModeScreen,
};

/// <summary>
/// ルートシグネチャの種類を表す
/// </summary>
enum class RootSignatureType {
  Object3D,
  Object3DInstancing,
  Sprite,
  Particle,
  FogOverlay,
  Primitive3D,
};

/// <summary>
/// パイプライン構築時のオプションを保持する
/// </summary>
struct GPipelineOptions {
  bool enableAlphaBlend = false; // ← スプライトは true
  bool enableDepth = true;       // ← スプライトは false
  bool enableDepthWrite = true;  // 深度を書き込むかどうか
  D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK;
  D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID;
  BlendMode blendMode = kBlendModeNormal;
  RootSignatureType rootType = RootSignatureType::Object3D;
  D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType =
      D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
};

/// <summary>
/// ルートシグネチャと PSO を構築する
/// </summary>
class GraphicsPipeline {
public:
  /// <summary>
  /// GraphicsPipeline を初期化する
  /// </summary>
  void Init(ID3D12Device *device) { device_ = device; }

  /// <summary>
  /// GraphicsPipeline を終了する
  /// </summary>
  void Term();

  /// <summary>
  /// ルートシグネチャと PSO を構築する
  /// </summary>
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
             DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
             D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);

  /// <summary>
  /// ルートシグネチャと PSO を構築する（ID3DBlob 版）
  /// </summary>
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             ID3DBlob *vs, ID3DBlob *ps, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
             D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);

  /// <summary>
  /// ルートシグネチャと PSO を構築する（IDxcBlob 版）
  /// </summary>
  void Build(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
             struct IDxcBlob *vs, struct IDxcBlob *ps, DXGI_FORMAT rtvFmt,
             DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK,
             D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID);

  /// <summary>
  /// 拡張オプション付きで PSO を構築する
  /// </summary>
  void BuildEx(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
               D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
               DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
               const GPipelineOptions &opt);

  /// <summary>
  /// ルートシグネチャを取得する
  /// </summary>
  ID3D12RootSignature *Root() const { return root_; }

  /// <summary>
  /// PSO を取得する
  /// </summary>
  ID3D12PipelineState *PSO() const { return pso_; }

private:
  void
  buildRootSignature_(RootSignatureType type = RootSignatureType::Object3D);
  void buildPSO_(const D3D12_INPUT_ELEMENT_DESC *inputElems, UINT elemCount,
                 D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps,
                 DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt, D3D12_CULL_MODE cull,
                 D3D12_FILL_MODE fill);

private:
  ID3D12Device *device_ = nullptr;      // 非所有
  ID3D12RootSignature *root_ = nullptr; // 所有
  ID3D12PipelineState *pso_ = nullptr;  // 所有
};
