#pragma once
#include <string_view>
#include <vector>

#include <d3d12.h>

#include "Math/Math.h"
#include "struct.h"
#include "Scene.h"

// 3D描画実装をファイル分割するための「内部窓口」。
// - RenderCommon.cpp が内部状態を保持
// - ModelRender.cpp など、内部レンダラーがそれを参照する
// ※運用上は「Internal」フォルダに置く / include path を限定する等で
//   外部から include されないようにするのがおすすめ。

class TextureManager;
class GraphicsPipeline;

namespace RC {

//struct SceneContext;

class DirectionalLightManager;
class PointLightManager;
class SpotLightManager;
class AreaLightManager;

namespace detail {

// ------------------------------
// Core state access
// ------------------------------
bool IsInitialized();
ID3D12Device *GetDevice();
ID3D12GraphicsCommandList *GetCL();

const Matrix4x4 &GetView();
const Matrix4x4 &GetProj();

TextureManager &GetTexMan();

DirectionalLightManager &GetDirLightMan();
PointLightManager &GetPointLightMan();
SpotLightManager &GetSpotLightMan();
AreaLightManager &GetAreaLightMan();

GraphicsPipeline *BindPipeline(std::string_view prefix);

void BindCameraCB();
void BindPointLightCB();
void BindSpotLightCB();
void BindAreaLightCB();

// ------------------------------
// Extension hooks
// - RenderCommon 側は「モデル」等を一切知らず、ここに登録された処理を呼ぶだけ。
// - これで RenderCommon.cpp から完全に Model/Sphere 依存を消せる。
// ------------------------------
using InitHook = void (*)(SceneContext &ctx);
using TermHook = void (*)();
using PreDraw3DHook = void (*)();

void RegisterInitHook(InitHook hook);
void RegisterTermHook(TermHook hook);
void RegisterPreDraw3DHook(PreDraw3DHook hook);

// RenderCommon.cpp 側から呼ぶ（外部モジュールからは呼ばない想定）
void CallInitHooks(SceneContext &ctx);
void CallTermHooks();
void CallPreDraw3DHooks();

} // namespace detail
} // namespace RC
