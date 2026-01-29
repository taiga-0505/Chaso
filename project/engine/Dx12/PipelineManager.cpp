#include "PipelineManager.h"
#include <cassert>

void PipelineManager::Init(ID3D12Device *device, DXGI_FORMAT rtvFmt,
                           DXGI_FORMAT dsvFmt) {
  // ====================
  // Device
  // ====================
  // デバイス設定
  device_ = device;
  rtvFmt_ = rtvFmt;
  dsvFmt_ = dsvFmt;

  // ====================
  // Compiler
  // ====================
  // シェーダーコンパイラ初期化
  const bool ok = compiler_.Init();
  assert(ok && "ShaderCompiler::Init failed");
}

void PipelineManager::Term() {
  // ====================
  // Pipeline Release
  // ====================
  // 登録済みパイプライン解放
  for (auto &kv : pipelines_) {
    if (kv.second.pipeline)
      kv.second.pipeline->Term();
  }
  pipelines_.clear();
  device_ = nullptr;
}

GraphicsPipeline *PipelineManager::Create(const std::string &key,
                                          const PipelineDesc &desc) {
  // ====================
  // Validate
  // ====================
  // シェーダーパス確認
  assert(!desc.vsPath.empty());
  assert(!desc.psPath.empty());

  // ====================
  // Compile
  // ====================
  // シェーダー設定
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

  // コンパイル
  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  assert(VS.HasBlob() && PS.HasBlob());

  // ====================
  // Create
  // ====================
  // パイプライン作成
  return createFromBlobs_(key, desc, VS.Blob(), PS.Blob());
}

GraphicsPipeline *PipelineManager::CreateFromFiles(
    const std::string &key, const std::wstring &vsPath,
    const std::wstring &psPath, InputLayoutType layoutType,
    const GPipelineOptions &opt) {
  // ====================
  // Desc
  // ====================
  // パイプライン設定生成
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

  // ====================
  // Create
  // ====================
  // パイプライン作成
  return Create(key, d);
}

GraphicsPipeline *PipelineManager::Get(const std::string &key) {
  // ====================
  // Find
  // ====================
  // 登録済みパイプライン取得
  auto it = pipelines_.find(key);
  return (it == pipelines_.end()) ? nullptr : it->second.pipeline.get();
}

bool PipelineManager::Rebuild(const std::string &key) {
  // ====================
  // Find
  // ====================
  // 対象パイプライン取得
  auto it = pipelines_.find(key);
  if (it == pipelines_.end())
    return false;

  const PipelineDesc &d = it->second.desc;

  // ====================
  // Compile
  // ====================
  // シェーダー設定
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

  // コンパイル
  CompiledShader VS = compiler_.Compile(vs);
  CompiledShader PS = compiler_.Compile(ps);
  if (!VS.HasBlob() || !PS.HasBlob())
    return false;

  // ====================
  // Rebuild
  // ====================
  // パイプライン再構築
  it->second.pipeline->Term();
  it->second.pipeline->Init(device_);

  D3D12_SHADER_BYTECODE vsBC{VS.Blob()->GetBufferPointer(),
                             VS.Blob()->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{PS.Blob()->GetBufferPointer(),
                             PS.Blob()->GetBufferSize()};

  const D3D12_INPUT_ELEMENT_DESC *il =
      d.inputLayout.empty() ? nullptr : d.inputLayout.data();
  it->second.pipeline->BuildEx(il, static_cast<UINT>(d.inputLayout.size()),
                               vsBC, psBC, rtvFmt_, dsvFmt_, d.opt);

  return true;
}

void PipelineManager::RebuildAll() {
  // ====================
  // Rebuild
  // ====================
  // 全パイプライン再ビルド
  for (auto &kv : pipelines_) {
    (void)Rebuild(kv.first);
  }
}

GraphicsPipeline *PipelineManager::createFromBlobs_(const std::string &key,
                                                    const PipelineDesc &desc,
                                                    IDxcBlob *vs,
                                                    IDxcBlob *ps) {
  // ====================
  // Create
  // ====================
  // パイプライン登録
  Entry e;
  e.desc = desc;
  e.pipeline = std::make_unique<GraphicsPipeline>();
  e.pipeline->Init(device_);

  D3D12_SHADER_BYTECODE vsBC{vs->GetBufferPointer(), vs->GetBufferSize()};
  D3D12_SHADER_BYTECODE psBC{ps->GetBufferPointer(), ps->GetBufferSize()};

  const D3D12_INPUT_ELEMENT_DESC *il =
      e.desc.inputLayout.empty() ? nullptr : e.desc.inputLayout.data();
  e.pipeline->BuildEx(il, static_cast<UINT>(e.desc.inputLayout.size()), vsBC,
                      psBC, rtvFmt_, dsvFmt_, e.desc.opt);

  auto &ref = pipelines_[key] = std::move(e);
  return ref.pipeline.get();
}

std::vector<D3D12_INPUT_ELEMENT_DESC>
PipelineManager::MakeInputLayout(InputLayoutType type) {
  // ====================
  // InputLayout
  // ====================
  // レイアウト生成
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

  case InputLayoutType::Primitive3D:
    return {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D12_APPEND_ALIGNED_ELEMENT,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

  case InputLayoutType::None:
    return {};
  }
  return {};
}

std::string PipelineManager::MakeKey(std::string_view prefix, BlendMode mode) {
  // ====================
  // Key
  // ====================
  // ブレンドモード別キー生成
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
  case kBlendModePremultiplied:
    suffix = "premul";
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
  // ====================
  // Shader Paths
  // ====================
  // 既定シェーダーパス
  const std::wstring objVs =
      L"Resources/Shader/Object3d/Object3D_Single.VS.hlsl";
  const std::wstring objVsInst =
      L"Resources/Shader/Object3d/Object3D_Inst.VS.hlsl";
  const std::wstring objPs = L"Resources/Shader/Object3d/Object3D.PS.hlsl";
  const std::wstring glassPs =
      L"Resources/Shader/Object3d/Object3D_Glass.PS.hlsl";
  const std::wstring sprVs = L"Resources/Shader/Sprite/Sprite.VS.hlsl";
  const std::wstring sprPs = L"Resources/Shader/Sprite/Sprite.PS.hlsl";
  const std::wstring ptlVs = L"Resources/Shader/Particle/Particle.VS.hlsl";
  const std::wstring ptlPs = L"Resources/Shader/Particle/Particle.PS.hlsl";
  const std::wstring primVs =
      L"Resources/Shader/Primitive2D/Primitive2D.VS.hlsl";
  const std::wstring primPs =
      L"Resources/Shader/Primitive2D/Primitive2D.PS.hlsl";
  const std::wstring prim3dVs =
      L"Resources/Shader/Primitive3D/Primitive3D.VS.hlsl";
  const std::wstring prim3dPs =
      L"Resources/Shader/Primitive3D/Primitive3D.PS.hlsl";

  // fullscreen fog overlay (cold mist)
  const std::wstring fogFx = L"Resources/Shader/Post/FogOverlay.hlsl";

  // ====================
  // Helpers
  // ====================
  // 登録ヘルパー
  auto regSet = [&](std::string_view prefix, const std::wstring &vs,
                    const std::wstring &ps, InputLayoutType layout,
                    RootSignatureType root, bool depth, bool depthWrite,
                    D3D12_CULL_MODE cull) {
    for (int m = (int)kBlendModeNone; m <= (int)kBlendModePremultiplied; ++m) {
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

  auto regPrim3D = [&](std::string_view prefix, bool depth) {
    for (int m = (int)kBlendModeNone; m <= (int)kBlendModeScreen; ++m) {
      const BlendMode mode = (BlendMode)m;

      GPipelineOptions opt{};
      opt.rootType = RootSignatureType::Primitive3D;
      opt.enableDepth = depth;
      opt.enableDepthWrite = false;
      opt.enableAlphaBlend = (mode != kBlendModeNone);
      opt.blendMode = mode;
      opt.cull = D3D12_CULL_MODE_NONE;
      opt.topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

      CreateFromFiles(MakeKey(prefix, mode), prim3dVs, prim3dPs,
                      InputLayoutType::Primitive3D, opt);
    }
  };

  // ====================
  // Register
  // ====================
  // object3d：深度ON、書き込みON、基本BACKカリング
  regSet("object3d", objVs, objPs, InputLayoutType::Object3D,
         RootSignatureType::Object3D, true, true, D3D12_CULL_MODE_BACK);

  // object3d_nocull：深度ON、書き込みON、カリング無し
  regSet("object3d_nocull", objVs, objPs, InputLayoutType::Object3D,
         RootSignatureType::Object3D, true, true, D3D12_CULL_MODE_NONE);

  regSet("object3d_inst", objVsInst, objPs, InputLayoutType::Object3D,
         RootSignatureType::Object3DInstancing, true, true,
         D3D12_CULL_MODE_BACK);

  // sprite：深度OFF、基本BACK（必要なら NONE に）
  regSet("sprite", sprVs, sprPs, InputLayoutType::Sprite,
         RootSignatureType::Sprite, false, false, D3D12_CULL_MODE_BACK);

  // particle：深度ON、書き込みOFF（積む用）
  regSet("particle", ptlVs, ptlPs, InputLayoutType::Particle,
         RootSignatureType::Particle, true, false, D3D12_CULL_MODE_BACK);

  // 汎用2D：基本は画面オーバーレイ想定
  regSet("primitive2d", primVs, primPs, InputLayoutType::Sprite,
         RootSignatureType::Sprite, false, false, D3D12_CULL_MODE_NONE);

  // 汎用3Dライン：深度ON版
  regPrim3D("primitive3d", true);

  // 汎用3Dライン：深度OFF版
  regPrim3D("primitive3d_nodepth", false);

  // fog overlay：深度OFF、αブレンドON、InputLayout無し、Rootは FogOverlay
  {
    PipelineDesc d{};
    d.vsPath = fogFx;
    d.psPath = fogFx;
    d.vsEntry = L"VS";
    d.psEntry = L"PS";
    d.inputLayout = MakeInputLayout(InputLayoutType::None);

    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::FogOverlay;
    opt.enableDepth = false;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModeNormal;
    opt.cull = D3D12_CULL_MODE_NONE;
    opt.fill = D3D12_FILL_MODE_SOLID;
    d.opt = opt;

#ifdef _DEBUG
    d.optimize = false;
    d.debugInfo = true;
#else
    d.optimize = true;
    d.debugInfo = false;
#endif

    Create(MakeKey("fog", kBlendModeNormal), d);
  }

  // 単体
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3D;
    opt.enableDepth = true;
    opt.enableDepthWrite = false; // ←重要
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied; // ←重要
    opt.cull = D3D12_CULL_MODE_BACK;         // 窓板みたいな薄い板ならおすすめ

    CreateFromFiles(MakeKey("object3d_glass", kBlendModePremultiplied), objVs,
                    glassPs, InputLayoutType::Object3D, opt);
  }

  // 2パス用（背面描画 = FRONTカリング
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3D;
    opt.enableDepth = true;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied;
    opt.cull = D3D12_CULL_MODE_FRONT; // ←ここが追加ポイント

    CreateFromFiles(MakeKey("object3d_glass_front", kBlendModePremultiplied),
                    objVs, glassPs, InputLayoutType::Object3D, opt);
  }

  // instancing版も欲しければ
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3DInstancing;
    opt.enableDepth = true;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied;
    opt.cull = D3D12_CULL_MODE_BACK;

    CreateFromFiles(MakeKey("object3d_glass_inst", kBlendModePremultiplied),
                    objVsInst, glassPs, InputLayoutType::Object3D, opt);
  }

  // instancing背面用
  {
    GPipelineOptions opt{};
    opt.rootType = RootSignatureType::Object3DInstancing;
    opt.enableDepth = true;
    opt.enableDepthWrite = false;
    opt.enableAlphaBlend = true;
    opt.blendMode = kBlendModePremultiplied;
    opt.cull = D3D12_CULL_MODE_FRONT;

    CreateFromFiles(
        MakeKey("object3d_glass_inst_front", kBlendModePremultiplied),
        objVsInst, glassPs, InputLayoutType::Object3D, opt);
  }
}
