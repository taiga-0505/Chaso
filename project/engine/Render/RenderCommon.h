#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "Math/Math.h"
#include "Scene.h"
#include "Sphere/Sphere.h"
#include <Model3D/ModelMesh.h>
#include <Model3D/ModelObject.h>
#include <d3d12.h>
#include <string>
#include "struct.h"

// D3D12 GPUハンドルを返すために必要
struct D3D12_GPU_DESCRIPTOR_HANDLE;

namespace RC {

/// <summary>
/// RenderCommon を初期化する（起動時に一度だけ呼ぶ）
/// </summary>
/// <param name="ctx">SceneContext</param>
/// <remarks>
/// App::Init 内など、Device / PipelineManager / SRVManager が有効になった後に呼んでください。
/// </remarks>
void Init(SceneContext &ctx);

/// <summary>
/// RenderCommon を終了する（確保したリソースを解放）
/// </summary>
void Term();

/// <summary>
/// 今フレームのカメラ情報（View/Proj とカメラ位置）を共有する
/// </summary>
/// <param name="view">ビュー行列</param>
/// <param name="proj">プロジェクション行列</param>
/// <param name="camWorldPos">カメラのワールド座標</param>
/// <remarks>
/// View/Proj は各オブジェクト Update に渡され、camWorldPos はシェーダー側で利用できます。
/// </remarks>
void SetCamera(const Matrix4x4 &view, const Matrix4x4 &proj,
               const RC::Vector3 camWorldPos);

// ==============================
// ライト用
// ==============================

class Light;

/// <summary>
/// ライトを生成してハンドルを返す
/// </summary>
/// <returns>ライトハンドル（失敗時は -1）</returns>
int CreateLight();

/// <summary>
/// ライトを破棄する
/// </summary>
/// <param name="lightHandle">ライトハンドル</param>
void DestroyLight(int lightHandle);

/// <summary>
/// 3D描画で使用する「アクティブライト」を切り替える
/// </summary>
/// <param name="lightHandle">ライトハンドル</param>
void SetActiveLight(int lightHandle);

/// <summary>
/// 現在のアクティブライトのハンドルを返す
/// </summary>
/// <returns>ライトハンドル（未設定なら -1）</returns>
int GetActiveLightHandle();

/// <summary>
/// ライトの実体ポインタを取得する
/// </summary>
/// <param name="lightHandle">ライトハンドル</param>
/// <returns>Light*（無効ハンドルなら nullptr）</returns>
Light *GetLightPtr(int lightHandle);

/// <summary>
/// ライトの ImGui 表示を行う
/// </summary>
/// <param name="lightHandle">ライトハンドル</param>
/// <param name="name">表示名</param>
void DrawImGuiLight(int lightHandle, const char *name);

// ==============================
// モデル用
// ==============================

/// <summary>
/// 3D描画の前処理（このフレームの SceneContext / CommandList を登録）
/// </summary>
/// <param name="ctx">SceneContext</param>
/// <param name="cl">このフレームで使うコマンドリスト</param>
/// <remarks>
/// この関数を呼んだ後に DrawModel / DrawSphere を呼んでください。
/// </remarks>
void PreDraw3D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

/// <summary>
/// モデルをロードしてハンドルを返す（Mesh は内部キャッシュで共有）
/// </summary>
/// <param name="path">.obj へのパス</param>
/// <returns>モデルハンドル（失敗時は -1）</returns>
int LoadModel(const std::string &path);

/// <summary>
/// モデルを描画する（テクスチャ指定版）
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <param name="texHandle">テクスチャハンドル（RC::LoadTex で取得）</param>
void DrawModel(int modelHandle, int texHandle);

/// <summary>
/// モデルを描画する（モデルが参照する mtl のテクスチャを使用）
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
void DrawModel(int modelHandle);

/// <summary>
/// モデルを描画する（カリング無効版）
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <param name="texHandle">テクスチャハンドル（-1 なら mtl のテクスチャ）</param>
void DrawModelNoCull(int modelHandle, int texHandle = -1);

/// <summary>
/// モデルをインスタンシング（複数Transform）で描画する
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <param name="instances">インスタンスTransform配列</param>
/// <param name="texHandle">テクスチャハンドル（-1 なら mtl のテクスチャ）</param>
void DrawModelBatch(int modelHandle, const std::vector<Transform> &instances,
                    int texHandle = -1);

/// <summary>
/// モデルの ImGui 表示を行う
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <param name="name">表示名</param>
void DrawImGui3D(int modelHandle, const char *name);

/// <summary>
/// モデルを解放する（ハンドルは無効化される）
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
void UnloadModel(int modelHandle);

/// <summary>
/// モデルの Transform ポインタを取得する
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <returns>Transform*（無効ハンドルなら nullptr）</returns>
Transform *GetModelTransformPtr(int modelHandle);

/// <summary>
/// モデルの色（乗算カラー）を設定する
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <param name="color">色（RGBA）</param>
void SetModelColor(int modelHandle, const Vector4 &color);

/// <summary>
/// モデルのライティングモードを設定する
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <param name="m">ライティングモード</param>
void SetModelLightingMode(int modelHandle, LightingMode m);

/// <summary>
/// モデルが参照する Mesh を差し替える（同じ .obj は内部キャッシュで共有）
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
/// <param name="path">.obj へのパス</param>
void SetModelMesh(int modelHandle, const std::string &path);

/// <summary>
/// DrawModelBatch の内部カーソルをリセットする
/// </summary>
/// <param name="modelHandle">モデルハンドル</param>
void ResetCursor(int modelHandle);

// ===============================
// 2D用
// ===============================

/// <summary>
/// 2D描画の前処理（このフレームの SceneContext / CommandList を登録）
/// </summary>
/// <param name="ctx">SceneContext</param>
/// <param name="cl">このフレームで使うコマンドリスト</param>
/// <remarks>
/// この関数を呼んだ後に DrawSprite / DrawLine などを呼んでください。
/// </remarks>
void PreDraw2D(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

/// <summary>
/// スプライト用テクスチャをロードし、Sprite2D を生成してハンドルを返す
/// </summary>
/// <param name="path">画像パス</param>
/// <param name="ctx">SceneContext（画面サイズ取得に使用）</param>
/// <param name="srgb">sRGBとして読み込むか</param>
/// <returns>スプライトハンドル（失敗時は -1）</returns>
int LoadSprite(const std::string &path, SceneContext &ctx, bool srgb = true);

/// <summary>
/// スプライトを描画する
/// </summary>
/// <param name="spriteHandle">スプライトハンドル</param>
void DrawSprite(int spriteHandle);

/// <summary>
/// スプライトを「テクスチャ内の矩形」を指定して描画する（スプライトシート用）
/// </summary>
/// <param name="spriteHandle">スプライトハンドル</param>
/// <param name="srcX">切り出し矩形 左上X（ピクセル, 0=左）</param>
/// <param name="srcY">切り出し矩形 左上Y（ピクセル, 0=上）</param>
/// <param name="srcW">切り出し幅（ピクセル）</param>
/// <param name="srcH">切り出し高さ（ピクセル）</param>
/// <param name="texW">テクスチャ全体の幅（ピクセル）</param>
/// <param name="texH">テクスチャ全体の高さ（ピクセル）</param>
/// <param name="insetPx">
/// にじみ対策の内側オフセット（ピクセル）。
/// 例: 線形サンプラーで隣のセルが混ざる時は 0.5f を試す。
/// </param>
/// <remarks>
/// - 位置/回転/サイズは SetSpriteTransform で設定した値を使います。
/// - この関数は「この呼び出しだけ」UV
/// を差し替えて描画し、描画後に元に戻します。
/// </remarks>
void DrawSpriteRect(int spriteHandle, float srcX, float srcY, float srcW,
                    float srcH, float texW, float texH, float insetPx = 0.0f);

/// <summary>
/// スプライトを UV(0..1) の矩形を指定して描画する
/// </summary>
/// <remarks>
/// DrawSpriteRect の「UV版」。テクスチャのピクセルサイズが分からない時に便利。
/// </remarks>
void DrawSpriteRectUV(int spriteHandle, float u0, float v0, float u1, float v1);

/// <summary>
/// スプライトの Transform を設定する
/// </summary>
/// <param name="spriteHandle">スプライトハンドル</param>
/// <param name="t">Transform</param>
void SetSpriteTransform(int spriteHandle, const Transform &t);

/// <summary>
/// スプライトの色（乗算カラー）を設定する
/// </summary>
/// <param name="spriteHandle">スプライトハンドル</param>
/// <param name="color">色（RGBA）</param>
void SetSpriteColor(int spriteHandle, const Vector4 &color);

/// <summary>
/// スプライトを解放する
/// </summary>
/// <param name="spriteHandle">スプライトハンドル</param>
void UnloadSprite(int spriteHandle);

/// <summary>
/// スプライトのサイズを設定する
/// </summary>
/// <param name="spriteHandle">スプライトハンドル</param>
/// <param name="w">幅（ピクセル）</param>
/// <param name="h">高さ（ピクセル）</param>
void SetSpriteScreenSize(int spriteHandle, float w, float h);

/// <summary>
/// スプライトの ImGui 表示を行う
/// </summary>
/// <param name="spriteHandle">スプライトハンドル</param>
/// <param name="name">表示名</param>
void DrawImGui2D(int spriteHandle, const char *name);

// ===============================
// 天球用
// ===============================

/// <summary>
/// 天球を生成する（最小形：半径0.5, 16x16, 内向き）
/// </summary>
/// <param name="textureHandle">テクスチャハンドル（必須）</param>
/// <returns>天球ハンドル（失敗時は -1）</returns>
/// <remarks>
/// キューブマップ等のスカイ用。必ず有効なテクスチャハンドルを渡してください。
/// </remarks>
int GenerateSphere(int textureHandle);

/// <summary>
/// 天球を生成する（パラメータ指定版）
/// </summary>
/// <param name="textureHandle">テクスチャハンドル（-1 なら未設定）</param>
/// <param name="radius">半径</param>
/// <param name="sliceCount">スライス数</param>
/// <param name="stackCount">スタック数</param>
/// <param name="inward">内向き（true）/ 外向き（false）</param>
/// <returns>天球ハンドル（失敗時は -1）</returns>
int GenerateSphereEx(int textureHandle = -1, float radius = 0.5f,
                     unsigned int sliceCount = 16, unsigned int stackCount = 16,
                     bool inward = true);

/// <summary>
/// 天球を描画する
/// </summary>
/// <param name="sphereHandle">天球ハンドル</param>
/// <param name="texHandle">一時的に差し替えるテクスチャ（-1 なら生成時のテクスチャ）</param>
void DrawSphere(int sphereHandle, int texHandle = -1);

/// <summary>
/// 天球の ImGui 表示を行う
/// </summary>
/// <param name="sphereHandle">天球ハンドル</param>
/// <param name="name">表示名（nullptr ならデフォルト）</param>
void DrawSphereImGui(int sphereHandle, const char *name = nullptr);

/// <summary>
/// 天球を解放する
/// </summary>
/// <param name="sphereHandle">天球ハンドル</param>
void UnloadSphere(int sphereHandle);

/// <summary>
/// 天球の Transform ポインタを取得する
/// </summary>
/// <param name="sphereHandle">天球ハンドル</param>
/// <returns>Transform*（無効ハンドルなら nullptr）</returns>
Transform *GetSphereTransformPtr(int sphereHandle);

/// <summary>
/// 天球の色（乗算カラー）を設定する
/// </summary>
/// <param name="sphereHandle">天球ハンドル</param>
/// <param name="color">色（RGBA）</param>
void SetSphereColor(int sphereHandle, const Vector4 &color);

/// <summary>
/// 天球のライティングモードを設定する
/// </summary>
/// <param name="sphereHandle">天球ハンドル</param>
/// <param name="m">ライティングモード</param>
void SetSphereLightingMode(int sphereHandle, LightingMode m);

// ===============================
// Primitive2D（即時描画）
// ===============================

/// <summary>
/// 線描画
/// </summary>
/// <param name="pos1">座標1（画面ピクセル座標）</param>
/// <param name="pos2">座標2（画面ピクセル座標）</param>
/// <param name="color">色（RGBA）</param>
/// <param name="thickness">線の太さ（ピクセル）</param>
/// <param name="feather">アンチエイリアス幅（ピクセル）</param>
void DrawLine(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
              float thickness = 1.0f, float feather = 1.0f);

/// <summary>
/// 四角形描画
/// </summary>
/// <param name="pos1">座標1（画面ピクセル座標）</param>
/// <param name="pos2">座標2（画面ピクセル座標）</param>
/// <param name="color">色（RGBA）</param>
/// <param name="fillMode">塗りつぶし設定</param>
/// <param name="feather">アンチエイリアス幅（ピクセル）</param>
void DrawBox(const Vector2 &pos1, const Vector2 &pos2, const Vector4 &color,
             kFillMode fillMode = kFill, float feather = 1.0f);

/// <summary>
/// 円描画
/// </summary>
/// <param name="center">円の中心点（画面ピクセル座標）</param>
/// <param name="radius">円の半径（ピクセル）</param>
/// <param name="color">色（RGBA）</param>
/// <param name="fillMode">塗りつぶし設定</param>
/// <param name="feather">アンチエイリアス幅（ピクセル）</param>
void DrawCircle(const Vector2 &center, float radius, const Vector4 &color,
                kFillMode fillMode = kFill, float feather = 1.0f);

/// <summary>
/// 三角形描画
/// </summary>
/// <param name="pos1">頂点1（画面ピクセル座標）</param>
/// <param name="pos2">頂点2（画面ピクセル座標）</param>
/// <param name="pos3">頂点3（画面ピクセル座標）</param>
/// <param name="color">色（RGBA）</param>
/// <param name="fillMode">塗りつぶし設定</param>
/// <param name="feather">アンチエイリアス幅（ピクセル）</param>
void DrawTriangle(const Vector2 &pos1, const Vector2 &pos2, const Vector2 &pos3,
                  const Vector4 &color, kFillMode fillMode = kFill,
                  float feather = 1.0f);

// ===============================
// 共通関数
// ===============================

/// <summary>
/// テクスチャをロードしてハンドルを返す
/// </summary>
/// <param name="path">画像パス</param>
/// <param name="srgb">sRGBとして読み込むか</param>
/// <returns>テクスチャハンドル（失敗時は -1）</returns>
int LoadTex(const std::string &path, bool srgb = true);

/// <summary>
/// テクスチャハンドルから SRV の GPU デスクリプタハンドルを取得する
/// </summary>
/// <param name="texHandle">テクスチャハンドル</param>
/// <returns>D3D12_GPU_DESCRIPTOR_HANDLE（無効なら ptr=0）</returns>
D3D12_GPU_DESCRIPTOR_HANDLE GetSrv(int texHandle);

/// <summary>
/// RenderCommon が保持している D3D12 Device を取得する
/// </summary>
ID3D12Device *GetDevice();

/// <summary>
/// RenderCommon が初期化済みかどうか
/// </summary>
bool IsInitialized();

// ===============================
// ブレンドモード切り替え
// ===============================

/// <summary>
/// 以降の Draw* が使用する BlendMode を設定する
/// </summary>
/// <param name="blendMode">ブレンドモード</param>
/// <remarks>
/// BindPipeline_ が prefix + BlendMode で PSO を選びます。
/// </remarks>
void SetBlendMode(BlendMode blendMode);

/// <summary>
/// 現在の BlendMode を取得する
/// </summary>
BlendMode GetBlendMode();

} // namespace RC
