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
};

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

class PipelineManager {
public:
  void Init(ID3D12Device *device, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt);
  void Term();

  // 登録（keyで管理）
  GraphicsPipeline *Create(const std::string &key, const PipelineDesc &desc);

  // 便利：layoutType から inputLayout を自動生成して登録
  GraphicsPipeline *CreateFromFiles(const std::string &key,
                                    const std::wstring &vsPath,
                                    const std::wstring &psPath,
                                    InputLayoutType layoutType,
                                    const GPipelineOptions &opt);

  // 取得
  GraphicsPipeline *Get(const std::string &key);

  // 再ビルド（登録時のdescを使ってコンパイルし直し）
  bool Rebuild(const std::string &key);
  void RebuildAll(); // まとめて

  // よく使うやつ：3系統×ブレンドを一括登録（キー規約もここで統一）
  void RegisterDefaultPipelines();

  // キー規約：prefix + "." + suffix
  // 例: object3d.normal / sprite.add / particle.mul
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
