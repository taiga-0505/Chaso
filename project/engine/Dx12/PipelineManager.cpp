#include "PipelineManager.h"
#include <cassert>

void PipelineManager::Init(ID3D12Device *device, DXGI_FORMAT rtvFmt,
                           DXGI_FORMAT dsvFmt) {
  device_ = device;
  rtvFmt_ = rtvFmt;
  dsvFmt_ = dsvFmt;

  const bool ok = compiler_.Init();
  assert(ok && "ShaderCompiler::Init failed");
}

void PipelineManager::Term() {
  for (auto &kv : pipelines_) {
    if (kv.second.pipeline)
      kv.second.pipeline->Term();
  }
  pipelines_.clear();
  device_ = nullptr;
}

GraphicsPipeline *PipelineManager::Create(const std::string &key,
                                          const PipelineDesc &desc) {
  assert(!desc.vsPath.empty());
  assert(!desc.psPath.empty());
  assert(!desc.inputLayout.empty());

  ShaderDesc vs{};
  vs.path = desc.vsPath.c_str();
  vs.entry = desc.vsEntry.c_str();
  vs.target = desc.vsTarget.c_str();
  vs.optimize = desc.optimize;
  vs.debugInfo = desc.debugInfo;

  ShaderDesc ps{};
  ps.path = desc.psPath.c_str();
  ps.entry = desc.psEntry.c_str();
  ps.target = desc.psTarget.c_str();
  ps.optimize = desc.optimize;
  ps.debugInfo = desc.debugInfo;

  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  assert(VS.HasBlob() && PS.HasBlob());

  return createFromBlobs_(key, desc, VS.Blob(), PS.Blob());
}

GraphicsPipeline *PipelineManager::CreateFromFiles(
    const std::string &key, const std::wstring &vsPath,
    const std::wstring &psPath, InputLayoutType layoutType,
    const GPipelineOptions &opt) {
  PipelineDesc d{};
  d.vsPath = vsPath;
  d.psPath = psPath;
  d.inputLayout = MakeInputLayout(layoutType);
  d.opt = opt;

#ifdef _DEBUG
  d.optimize = false;
  d.debugInfo = true;
#else
  d.optimize = true;
  d.debugInfo = false;
#endif

  return Create(key, d);
}

GraphicsPipeline *PipelineManager::Get(const std::string &key) {
  auto it = pipelines_.find(key);
  return (it == pipelines_.end()) ? nullptr : it->second.pipeline.get();
}

bool PipelineManager::Rebuild(const std::string &key) {
  auto it = pipelines_.find(key);
  if (it == pipelines_.end())
    return false;

  const PipelineDesc &d = it->second.desc;

  ShaderDesc vs{};
  vs.path = d.vsPath.c_str();
  vs.entry = d.vsEntry.c_str();
  vs.target = d.vsTarget.c_str();
  vs.optimize = d.optimize;
  vs.debugInfo = d.debugInfo;

  ShaderDesc ps{};
  ps.path = d.psPath.c_str();
  ps.entry = d.psEntry.c_str();
  ps.target = d.psTarget.c_str();
  ps.optimize = d.optimize;
  ps.debugInfo = d.debugInfo;

  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  if (!VS.HasBlob() || !PS.HasBlob())
    return false;

  it->second.pipeline->Term();
  it->second.pipeline->Init(device_);

  D3D12_SHADER_BYTECODE vsBC{VS.Blob()->GetBufferPointer(),
                             VS.Blob()->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{PS.Blob()->GetBufferPointer(),
                             PS.Blob()->GetBufferSize()};

  it->second.pipeline->BuildEx(d.inputLayout.data(),
                               static_cast<UINT>(d.inputLayout.size()), vsBC,
                               psBC, rtvFmt_, dsvFmt_, d.opt);

  return true;
}

void PipelineManager::RebuildAll() {
  for (auto &kv : pipelines_) {
    (void)Rebuild(kv.first);
  }
}

GraphicsPipeline *PipelineManager::createFromBlobs_(const std::string &key,
                                                    const PipelineDesc &desc,
                                                    IDxcBlob *vs,
                                                    IDxcBlob *ps) {
  Entry e;
  e.desc = desc;
  e.pipeline = std::make_unique<GraphicsPipeline>();
  e.pipeline->Init(device_);

  D3D12_SHADER_BYTECODE vsBC{vs->GetBufferPointer(), vs->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{ps->GetBufferPointer(), ps->GetBufferSize()};

  e.pipeline->BuildEx(e.desc.inputLayout.data(),
                      static_cast<UINT>(e.desc.inputLayout.size()), vsBC, psBC,
                      rtvFmt_, dsvFmt_, e.desc.opt);

  auto &ref = pipelines_[key] = std::move(e);
  return ref.pipeline.get();
}

std::vector<D3D12_INPUT_ELEMENT_DESC>
PipelineManager::MakeInputLayout(InputLayoutType type) {
  switch (type) {
  case InputLayoutType::Object3D:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::Sprite:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::Particle:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
  }
  return {};
}

std::string PipelineManager::MakeKey(std::string_view prefix, BlendMode mode) {
  const char *suffix = "normal";
  switch (mode) {
  case kBlendModeNone:
    suffix = "none";
    break;
  case kBlendModeNormal:
    suffix = "normal";
    break;
  case kBlendModeAdd:
    suffix = "add";
    break;
  case kBlendModeSubtract:
    suffix = "sub";
    break;
  case kBlendModeMultiply:
    suffix = "mul";
    break;
  case kBlendModeScreen:
    suffix = "screen";
    break;
  default:
    suffix = "normal";
    break;
  }

  std::string key;
  key.reserve(prefix.size() + 1 + 8);
  key.append(prefix);
  key.push_back('.');
  key.append(suffix);
  return key;
}

void PipelineManager::RegisterDefaultPipelines() {
  const std::wstring objVs = L"Resources/Shader/Object3d/Object3D.VS.hlsl";
  const std::wstring objPs = L"Resources/Shader/Object3d/Object3D.PS.hlsl";
  const std::wstring sprVs = L"Resources/Shader/Sprite/Sprite.VS.hlsl";
  const std::wstring sprPs = L"Resources/Shader/Sprite/Sprite.PS.hlsl";
  const std::wstring ptlVs = L"Resources/Shader/Particle/Particle.VS.hlsl";
  const std::wstring ptlPs = L"Resources/Shader/Particle/Particle.PS.hlsl";
  const std::wstring primVs =
      L"Resources/Shader/Primitive2D/Primitive2D.VS.hlsl";
  const std::wstring primPs =
      L"Resources/Shader/Primitive2D/Primitive2D.PS.hlsl";

  auto regSet = [&](std::string_view prefix, const std::wstring &vs,
                    const std::wstring &ps, InputLayoutType layout,
                    RootSignatureType root, bool depth, bool depthWrite,
                    D3D12_CULL_MODE cull) {
    for (int m = (int)kBlendModeNone; m <= (int)kBlendModeScreen; ++m) {
      const BlendMode mode = (BlendMode)m;

      GPipelineOptions opt{};
      opt.rootType = root;
      opt.enableDepth = depth;
      opt.enableDepthWrite = depthWrite;
      opt.enableAlphaBlend = (mode != kBlendModeNone);
      opt.blendMode = mode;
      opt.cull = cull;

      CreateFromFiles(MakeKey(prefix, mode), vs, ps, layout, opt);
    }
  };

  // object3d：深度ON、書き込みON、基本BACKカリング
  regSet("object3d", objVs, objPs, InputLayoutType::Object3D,
         RootSignatureType::Object3D, true, true, D3D12_CULL_MODE_BACK);

  // sprite：深度OFF、基本BACK（必要なら NONE に）
  regSet("sprite", sprVs, sprPs, InputLayoutType::Sprite,
         RootSignatureType::Sprite, false, false, D3D12_CULL_MODE_BACK);

  // particle：深度ON、書き込みOFF（積む用）
  regSet("particle", ptlVs, ptlPs, InputLayoutType::Particle,
         RootSignatureType::Particle, true, false, D3D12_CULL_MODE_BACK);

  // 汎用2D：基本は画面オーバーレイ想定
  regSet("primitive2d", primVs, primPs, InputLayoutType::Sprite,
         RootSignatureType::Sprite, false, false, D3D12_CULL_MODE_NONE);
}
