#include "PipelineManager.h"

void PipelineManager::Init(ID3D12Device *device, DXGI_FORMAT rtvFmt,
                           DXGI_FORMAT dsvFmt) {
  device_ = device;
  rtvFmt_ = rtvFmt;
  dsvFmt_ = dsvFmt;
  bool ok = compiler_.Init();
  assert(ok && "ShaderCompiler::Init failed");
}

void PipelineManager::Term() {
  // 生成済みPipelineを解放
  for (auto &kv : pipelines_) {
    if (kv.second.pipeline)
      kv.second.pipeline->Term();
  }
  pipelines_.clear();
  device_ = nullptr;
}

// HLSLファイルから作成（キーで管理）
GraphicsPipeline *PipelineManager::CreateFromFiles(const std::string &key,
                                                   const PipelineDesc &desc) {
  // シェーダをコンパイル
  ShaderDesc vs{};
  vs.path = desc.vsPath.c_str();
  vs.target = desc.vsTarget.c_str();
  vs.entry = desc.vsEntry.c_str();
  vs.optimize = desc.optimize;
  vs.debugInfo = desc.debugInfo;

  ShaderDesc ps{};
  ps.path = desc.psPath.c_str();
  ps.target = desc.psTarget.c_str();
  ps.entry = desc.psEntry.c_str();
  ps.optimize = desc.optimize;
  ps.debugInfo = desc.debugInfo;

  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  assert(VS.HasBlob() && PS.HasBlob());

  return createFromBlobs_(key, desc, VS.Blob(), PS.Blob());
}

GraphicsPipeline *PipelineManager::CreateFromFiles(const std::string &key,
                                                   const std::wstring &vsPath,
                                                   const std::wstring &psPath,
                                                   InputLayoutType layoutType) {
  PipelineDesc pdesc{};
  pdesc.vsPath = vsPath;
  pdesc.psPath = psPath;
  pdesc.inputLayout = GetInputLayout(layoutType);
#ifdef _DEBUG
  pdesc.optimize = false;
  pdesc.debugInfo = true;
#else
  pdesc.optimize = true;
  pdesc.debugInfo = false;
#endif
  return CreateFromFiles(key, pdesc);
}

bool PipelineManager::Rebuild(const std::string &key) {
  auto it = pipelines_.find(key);
  if (it == pipelines_.end())
    return false;
  const PipelineDesc &desc = it->second.desc;

  ShaderDesc vs{}, ps{};
  vs.path = desc.vsPath.c_str();
  vs.target = desc.vsTarget.c_str();
  vs.entry = desc.vsEntry.c_str();
  vs.optimize = desc.optimize;
  vs.debugInfo = desc.debugInfo;
  ps.path = desc.psPath.c_str();
  ps.target = desc.psTarget.c_str();
  ps.entry = desc.psEntry.c_str();
  ps.optimize = desc.optimize;
  ps.debugInfo = desc.debugInfo;

  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  if (!VS.HasBlob() || !PS.HasBlob())
    return false;

  // いったん解体して再Build
  it->second.pipeline->Term();
  it->second.pipeline->Init(device_);
  it->second.pipeline->Build(
      it->second.desc.inputLayout.data(),
      static_cast<UINT>(it->second.desc.inputLayout.size()), VS.Blob(),
      PS.Blob(), rtvFmt_, dsvFmt_, it->second.desc.cull, it->second.desc.fill);
  return true;
}

void PipelineManager::RegisterDefaultPipelines() {
  // ==============================
  // Object3D 用
  // ==============================
  const std::wstring objVs = L"Resources/Shader/Object3D.VS.hlsl";
  const std::wstring objPs = L"Resources/Shader/Object3D.PS.hlsl";

  // 互換用のベース PSO（必要なら）※App で "object3d" として作っていたもの
  CreateFromFiles("object3d", objVs, objPs, InputLayoutType::Object3D);

  // ブレンド違い
  CreateModelPipeline("ObjBlendModeNone", objVs, objPs, kBlendModeNone);
  CreateModelPipeline("ObjBlendModeNormal", objVs, objPs, kBlendModeNormal);
  CreateModelPipeline("ObjBlendModeAdd", objVs, objPs, kBlendModeAdd);
  CreateModelPipeline("ObjBlendModeSubtract", objVs, objPs, kBlendModeSubtract);
  CreateModelPipeline("ObjBlendModeMultiply", objVs, objPs, kBlendModeMultiply);
  CreateModelPipeline("ObjBlendModeScreen", objVs, objPs, kBlendModeScreen);

  // ==============================
  // Sprite 用
  // ==============================
  const std::wstring sprVs = L"Resources/Shader/Sprite.VS.hlsl";
  const std::wstring sprPs = L"Resources/Shader/Sprite.PS.hlsl";

  // 互換用のベース PSO（必要なら）※App で "sprite" として作っていたもの
  CreateFromFiles("sprite", sprVs, sprPs, InputLayoutType::Sprite);

  // ブレンド違い
  CreateSpritePipeline("BlendModeNone", sprVs, sprPs, kBlendModeNone);
  CreateSpritePipeline("BlendModeNormal", sprVs, sprPs, kBlendModeNormal);
  CreateSpritePipeline("BlendModeAdd", sprVs, sprPs, kBlendModeAdd);
  CreateSpritePipeline("BlendModeSubtract", sprVs, sprPs, kBlendModeSubtract);
  CreateSpritePipeline("BlendModeMultiply", sprVs, sprPs, kBlendModeMultiply);
  CreateSpritePipeline("BlendModeScreen", sprVs, sprPs, kBlendModeScreen);
}

GraphicsPipeline *PipelineManager::CreateModelPipeline(
    const std::string &key, const std::wstring &vsPath,
    const std::wstring &psPath, BlendMode mode) {
  // コンパイル（既存手順と同じ）
  ShaderDesc vs{}, ps{};
  vs.path = vsPath.c_str();
  vs.target = L"vs_6_0";
  vs.entry = L"main";
  ps.path = psPath.c_str();
  ps.target = L"ps_6_0";
  ps.entry = L"main";
#ifdef _DEBUG
  vs.optimize = ps.optimize = false;
  vs.debugInfo = ps.debugInfo = true;
#else
  vs.optimize = ps.optimize = true;
  vs.debugInfo = ps.debugInfo = false;
#endif
  auto VS = compiler_.Compile(vs);
  auto PS = compiler_.Compile(ps);
  assert(VS.HasBlob() && PS.HasBlob());

  // 入力レイアウト（既存の Object3D を使用）
  auto layout = GetInputLayout(InputLayoutType::Object3D);

  auto gp = std::make_unique<GraphicsPipeline>();
  gp->Init(device_);

  GPipelineOptions opt{};
  opt.enableAlphaBlend = (mode != kBlendModeNone); // None 以外はブレンドON
  opt.enableDepth = true;                          // モデルは深度ON
  opt.cull = D3D12_CULL_MODE_NONE;                 // 既定の背面カリング
  opt.blendMode = mode;

  D3D12_SHADER_BYTECODE vsBC{VS.Blob()->GetBufferPointer(),
                             VS.Blob()->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{PS.Blob()->GetBufferPointer(),
                             PS.Blob()->GetBufferSize()};
  gp->BuildEx(layout.data(), static_cast<UINT>(layout.size()), vsBC, psBC,
              rtvFmt_, dsvFmt_, opt);

  Entry e;
  e.desc.inputLayout = std::move(layout);
  e.pipeline = std::move(gp);
  auto &ref = pipelines_[key] = std::move(e);
  return ref.pipeline.get();
}

GraphicsPipeline *PipelineManager::GetModelPipeline(BlendMode mode) {
  // App 側で登録するキーと対応させる
  const char *key = "ObjBlendModeNormal";
  switch (mode) {
  case kBlendModeNone:
    key = "ObjBlendModeNone";
    break;
  case kBlendModeNormal:
    key = "ObjBlendModeNormal";
    break;
  case kBlendModeAdd:
    key = "ObjBlendModeAdd";
    break;
  case kBlendModeSubtract:
    key = "ObjBlendModeSubtract";
    break;
  case kBlendModeMultiply:
    key = "ObjBlendModeMultiply";
    break;
  case kBlendModeScreen:
    key = "ObjBlendModeScreen";
    break;
  }
  return Get(key);
}


GraphicsPipeline *
PipelineManager::CreateSpritePipeline(const std::wstring &vsPath,
                                                        const std::wstring& psPath) {
  // 1) シェーダをコンパイル（既存と同じ手順）
  ShaderDesc vs{}, ps{};
  vs.path = vsPath.c_str(); vs.target = L"vs_6_0"; vs.entry = L"main";
  ps.path = psPath.c_str(); ps.target = L"ps_6_0"; ps.entry = L"main";
#ifdef _DEBUG
  vs.optimize = ps.optimize = false; vs.debugInfo = ps.debugInfo = true;
#else
  vs.optimize = ps.optimize = true;  vs.debugInfo = ps.debugInfo = false;
#endif
  auto VS = compiler_.Compile(vs);
  auto PS = compiler_.Compile(ps);
  assert(VS.HasBlob() && PS.HasBlob());

  // 2) 入力レイアウト
  auto layout = GetInputLayout(InputLayoutType::Sprite);
  // 3) PSO 構築（BuildEx を使用）
  auto gp = std::make_unique<GraphicsPipeline>();
  gp->Init(device_);

  GPipelineOptions opt{};
  opt.enableAlphaBlend = true;              // 半透明ON
  opt.enableDepth = false;                  // 深度OFF
  opt.cull = D3D12_CULL_MODE_BACK; // カリングなし

  D3D12_SHADER_BYTECODE vsBC{VS.Blob()->GetBufferPointer(), VS.Blob()->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{PS.Blob()->GetBufferPointer(), PS.Blob()->GetBufferSize()};
  gp->BuildEx(layout.data(), static_cast<UINT>(layout.size()),
              vsBC, psBC, rtvFmt_, dsvFmt_, opt);

  // 4) マップには任意のキーで登録（ここでは "sprite"）
  Entry e;
  e.desc.inputLayout = std::move(layout);
  e.pipeline = std::move(gp);
  auto& ref = pipelines_["sprite"] = std::move(e);
  return ref.pipeline.get();
}

GraphicsPipeline *PipelineManager::CreateSpritePipeline(
    const std::string &key, const std::wstring &vsPath,
    const std::wstring &psPath, BlendMode mode) {
  // 既存のコンパイル手順は流用
  ShaderDesc vs{}, ps{};
  vs.path = vsPath.c_str();
  vs.target = L"vs_6_0";
  vs.entry = L"main";
  ps.path = psPath.c_str();
  ps.target = L"ps_6_0";
  ps.entry = L"main";
  auto VS = compiler_.Compile(vs);
  auto PS = compiler_.Compile(ps);
  assert(VS.HasBlob() && PS.HasBlob());

  auto layout = GetInputLayout(InputLayoutType::Sprite);

  auto gp = std::make_unique<GraphicsPipeline>();
  gp->Init(device_);

  GPipelineOptions opt{};
  opt.enableAlphaBlend = (mode != kBlendModeNone);
  opt.enableDepth = false;
  opt.cull = D3D12_CULL_MODE_BACK;
  opt.blendMode = mode;

  D3D12_SHADER_BYTECODE vsBC{VS.Blob()->GetBufferPointer(),
                             VS.Blob()->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{PS.Blob()->GetBufferPointer(),
                             PS.Blob()->GetBufferSize()};
  gp->BuildEx(layout.data(), static_cast<UINT>(layout.size()), vsBC, psBC,
              rtvFmt_, dsvFmt_, opt);

  Entry e;
  e.desc.inputLayout = std::move(layout);
  e.pipeline = std::move(gp);
  auto &ref = pipelines_[key] = std::move(e);
  return ref.pipeline.get();
}

GraphicsPipeline *PipelineManager::GetSpritePipeline(BlendMode mode) {
  const char *key = "BlendModeNormal";
  switch (mode) {
  case kBlendModeNone:
    key = "BlendModeNone";
    break;
  case kBlendModeNormal:
    key = "BlendModeNormal";
    break;
  case kBlendModeAdd:
    key = "BlendModeAdd";
    break;
  case kBlendModeSubtract:
    key = "BlendModeSubtract";
    break;
  case kBlendModeMultiply:
    key = "BlendModeMultiply";
    break;
  case kBlendModeScreen:
    key = "BlendModeScreen";
    break;
  }
  return Get(key);
}

GraphicsPipeline *PipelineManager::createFromBlobs_(const std::string &key,
                                                    const PipelineDesc &desc,
                                                    IDxcBlob *vs,
                                                    IDxcBlob *ps) {
  Entry e;
  e.desc = desc;
  e.pipeline = std::make_unique<GraphicsPipeline>();
  e.pipeline->Init(device_);
  e.pipeline->Build(e.desc.inputLayout.data(),
                    static_cast<UINT>(e.desc.inputLayout.size()), vs, ps,
                    rtvFmt_, dsvFmt_, e.desc.cull, e.desc.fill);
  auto &ref = pipelines_[key] = std::move(e);
  return ref.pipeline.get();
}

std::vector<D3D12_INPUT_ELEMENT_DESC>
PipelineManager::GetInputLayout(InputLayoutType type) {
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
  }

  return {};
}
