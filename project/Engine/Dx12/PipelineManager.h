#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "ShaderCompiler/ShaderCompiler.h"

#include <d3d12.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class InputLayoutType {
  Object3D,
  Sprite,
  Particle,
  Primitive3D,
  None, // InputLayout無し（SV_VertexID 等）
};

/// <summary>
/// パイプライン作成に必要な情報をまとめる
/// </summary>
struct PipelineDesc {
  // Shader files
  std::wstring vsPath;
  std::wstring psPath;

  // Compile settings
  std::wstring vsEntry = L"main";
  std::wstring psEntry = L"main";
  std::wstring vsTarget = L"vs_6_0";
  std::wstring psTarget = L"ps_6_0";
  bool optimize = true;
  bool debugInfo = false;

  // InputLayout（寿命問題を避けるため実体保持）
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

  // Root/Blend/Depth/Raster など
  GPipelineOptions opt{};
};

/// <summary>
/// GraphicsPipeline を登録・取得・再ビルドする
/// </summary>
class PipelineManager {
public:
  /// <summary>
  /// PipelineManager を初期化する
  /// </summary>
  void Init(ID3D12Device *device, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt);

  /// <summary>
  /// PipelineManager を終了する
  /// </summary>
  void Term();

  /// <summary>
  /// 指定キーでパイプラインを作成して登録する
  /// </summary>
  GraphicsPipeline *Create(const std::string &key, const PipelineDesc &desc);

  /// <summary>
  /// ファイル指定でパイプラインを作成して登録する
  /// </summary>
  GraphicsPipeline *CreateFromFiles(const std::string &key,
                                    const std::wstring &vsPath,
                                    const std::wstring &psPath,
                                    InputLayoutType layoutType,
                                    const GPipelineOptions &opt);

  /// <summary>
  /// 指定キーのパイプラインを取得する
  /// </summary>
  GraphicsPipeline *Get(const std::string &key);

  /// <summary>
  /// 指定キーのパイプラインを再ビルドする
  /// </summary>
  bool Rebuild(const std::string &key);

  /// <summary>
  /// 登録済みのパイプラインをすべて再ビルドする
  /// </summary>
  void RebuildAll();

  /// <summary>
  /// 既定のパイプライン群を登録する
  /// </summary>
  void RegisterDefaultPipelines();

  /// <summary>
  /// prefix + ブレンドモードからキーを作成する
  /// </summary>
  static std::string MakeKey(std::string_view prefix, BlendMode mode);

private:
  static std::vector<D3D12_INPUT_ELEMENT_DESC>
  MakeInputLayout(InputLayoutType type);

  GraphicsPipeline *createFromBlobs_(const std::string &key,
                                     const PipelineDesc &desc, IDxcBlob *vs,
                                     IDxcBlob *ps);

private:
  struct Entry {
    PipelineDesc desc;
    std::unique_ptr<GraphicsPipeline> pipeline;
  };

  ID3D12Device *device_ = nullptr; // 非所有
  DXGI_FORMAT rtvFmt_{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB};
  DXGI_FORMAT dsvFmt_{DXGI_FORMAT_D24_UNORM_S8_UINT};

  ShaderCompiler compiler_;
  std::unordered_map<std::string, Entry> pipelines_;
};
