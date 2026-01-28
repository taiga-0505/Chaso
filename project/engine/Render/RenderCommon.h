#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "Math/Math.h"
#include "Scene.h"
#include <d3d12.h>
#include <string>
#include "struct.h"

// D3D12 GPUハンドルを返すために必要
struct D3D12_GPU_DESCRIPTOR_HANDLE;

namespace RC {

// ============================================================================
// ライト生成時の「アクティブ登録」モード
// - None : 生成するだけ（描画には使われない）
// - Add  : 現在のアクティブ配列に追加（最大数を超えたら追加失敗）
// - Set  : アクティブ配列をクリアしてから追加（= それだけを使う）
// ============================================================================
enum class LightActivateMode {
  None = 0,
  Add,
  Set,
};


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

class DirectionalLightSource;

/// <summary>
/// ディレクショナルライトを生成してハンドルを返す
/// </summary>
/// <param name="activateMode">
/// - None: アクティブを変更しない
/// - Add : まだ明示アクティブが無い時だけアクティブにする
/// - Set : 必ずアクティブにする
/// </param>
/// <returns>ライトハンドル（失敗時は -1）</returns>
int CreateDirectionalLight(
    LightActivateMode activateMode = LightActivateMode::Set);

/// <summary>
/// ディレクショナルライトを破棄する
/// </summary>
/// <param name="lightHandle">ライトハンドル</param>
void DestroyDirectionalLight(int lightHandle);

/// <summary>
/// 3D描画で使用する「アクティブなディレクショナルライト」を切り替える
/// </summary>
/// <param name="lightHandle">
/// ライトハンドル。-1 の場合は「明示的なアクティブ無し」になり、
/// 描画時は共通のデフォルトライト（default slot）を使用します。
/// </param>
void SetActiveDirectionalLight(int lightHandle);

/// <summary>
/// 現在のアクティブなディレクショナルライトのハンドルを返す
/// </summary>
/// <returns>ライトハンドル（未設定なら default(0)）</returns>
int GetActiveDirectionalLightHandle();

/// <summary>
/// ディレクショナルライトの実体ポインタを取得する
/// </summary>
/// <param name="lightHandle">ライトハンドル</param>
/// <returns>DirectionalLightSource*（無効ハンドルなら nullptr）</returns>
DirectionalLightSource *GetDirectionalLightPtr(int lightHandle);

/// <summary>
/// ディレクショナルライトの ImGui 表示を行う
/// </summary>
/// <param name="lightHandle">ライトハンドル（-1
/// の場合は実効アクティブを使う）</param> <param name="name">表示名</param>
void DrawImGuiDirectionalLight(int lightHandle, const char *name);

/// <summary>
/// ディレクショナルライトの ON/OFF を切り替える
/// </summary>
void SetDirectionalLightEnabled(int lightHandle, bool enabled);

/// <summary>
/// ディレクショナルライトが ON かどうか
/// </summary>
bool IsDirectionalLightEnabled(int lightHandle);

/// <summary>
/// 今の「実効アクティブ」ディレクショナルライトの ON/OFF を切り替える
/// </summary>
void SetActiveDirectionalLightEnabled(bool enabled);

/// <summary>
/// 今の「実効アクティブ」ディレクショナルライトが ON かどうか
/// </summary>
bool IsActiveDirectionalLightEnabled();

// ==============================
// ポイントライト用

// ==============================

class PointLightSource;

/// <summary>
/// ポイントライトを生成してハンドルを返す
/// </summary>
/// <returns>ポイントライトハンドル（失敗時は -1）</returns>
int CreatePointLight(LightActivateMode activateMode = LightActivateMode::Add);

/// <summary>
/// ポイントライトを破棄する
/// </summary>
/// <param name="pointLightHandle">ポイントライトハンドル</param>
void DestroyPointLight(int pointLightHandle);

/// <summary>
/// 3D描画で使用する「アクティブポイントライト」を切り替える
/// </summary>
/// <param name="pointLightHandle">
/// ポイントライトハンドル。-1 の場合は「明示的なアクティブ無し」になり、
/// 描画時は共通のデフォルト（OFF）を使用します。
/// </param>
void SetActivePointLight(int pointLightHandle);

/// <summary>
/// 現在のアクティブポイントライトのハンドルを返す
/// </summary>
/// <returns>ポイントライトハンドル（未設定なら -1）</returns>
int GetActivePointLightHandle();

/// <summary>
/// アクティブポイントライト配列をクリアする
/// </summary>
void ClearActivePointLights();

/// <summary>
/// アクティブポイントライト配列に追加する
/// </summary>
bool AddActivePointLight(int pointLightHandle);

/// <summary>
/// アクティブポイントライト配列から削除する
/// </summary>
void RemoveActivePointLight(int pointLightHandle);

/// <summary>
/// アクティブポイントライトの数を取得する
/// </summary>
int GetActivePointLightCount();

/// <summary>
/// アクティブポイントライトのハンドルをインデックスで取得する
/// </summary>
int GetActivePointLightHandleAt(int index);


/// <summary>
/// ポイントライトの実体ポインタを取得する
/// </summary>
/// <param name="pointLightHandle">ポイントライトハンドル</param>
/// <returns>PointLightSource*（無効ハンドルなら nullptr）</returns>
PointLightSource *GetPointLightPtr(int pointLightHandle);

/// <summary>
/// ポイントライトの ImGui 表示を行う
/// </summary>
/// <param name="pointLightHandle">ポイントライトハンドル</param>
/// <param name="name">表示名</param>
void DrawImGuiPointLight(int pointLightHandle, const char *name);

/// <summary>
/// ポイントライトの ON/OFF を切り替える
/// </summary>
void SetPointLightEnabled(int pointLightHandle, bool enabled);

/// <summary>
/// ポイントライトが ON かどうか
/// </summary>
bool IsPointLightEnabled(int pointLightHandle);

/// <summary>
/// 今の「実効アクティブ」ポイントライトの ON/OFF を切り替える（先頭の active）
/// </summary>
void SetActivePointLightEnabled(bool enabled);

/// <summary>
/// 今の「実効アクティブ」ポイントライトが ON かどうか
/// </summary>
bool IsActivePointLightEnabled();


// ==============================
// スポットライト用
// ==============================

class SpotLightSource;

/// <summary>
/// スポットライトを生成してハンドルを返す
/// </summary>
/// <returns>スポットライトハンドル（失敗時は -1）</returns>
int CreateSpotLight(LightActivateMode activateMode = LightActivateMode::Add);

/// <summary>
/// スポットライトを破棄する
/// </summary>
/// <param name="spotLightHandle">スポットライトハンドル</param>
void DestroySpotLight(int spotLightHandle);

/// <summary>
/// 3D描画で使用する「アクティブスポットライト」を切り替える
/// </summary>
/// <param name="spotLightHandle">
/// スポットライトハンドル。-1 の場合は「明示的なアクティブ無し」になり、
/// 描画時は共通のデフォルト（OFF）を使用します。
/// </param>
void SetActiveSpotLight(int spotLightHandle);

/// <summary>
/// 現在のアクティブスポットライトのハンドルを返す
/// </summary>
/// <returns>スポットライトハンドル（未設定なら -1）</returns>
int GetActiveSpotLightHandle();

/// </summary>
/// アクティブスポットライト配列をクリアする
/// </summary>
void ClearActiveSpotLights();

/// <summary>
/// アクティブスポットライト配列に追加する
/// </summary>
bool AddActiveSpotLight(int spotLightHandle);

/// <summary>
/// アクティブスポットライト配列から削除する
/// </summary>
void RemoveActiveSpotLight(int spotLightHandle);

/// <summary>
/// アクティブスポットライトの数を取得する
/// </summary>
int GetActiveSpotLightCount();

/// <summary>
/// アクティブスポットライトのハンドルをインデックスで取得する
/// </summary>
int GetActiveSpotLightHandleAt(int index);

/// <summary>
/// スポットライトの実体ポインタを取得する
/// </summary>
/// <param name="spotLightHandle">スポットライトハンドル</param>
/// <returns>SpotLightSource*（無効ハンドルなら nullptr）</returns>
SpotLightSource *GetSpotLightPtr(int spotLightHandle);

/// <summary>
/// スポットライトの ImGui 表示を行う
/// </summary>
/// <param name="spotLightHandle">スポットライトハンドル</param>
/// <param name="name">表示名</param>
void DrawImGuiSpotLight(int spotLightHandle, const char *name);

/// <summary>
/// スポットライトの ON/OFF を切り替える
/// </summary>
void SetSpotLightEnabled(int spotLightHandle, bool enabled);

/// <summary>
/// スポットライトが ON かどうか
/// </summary>
bool IsSpotLightEnabled(int spotLightHandle);

/// <summary>
/// 今の「実効アクティブ」スポットライトの ON/OFF を切り替える（先頭の active）
/// </summary>
void SetActiveSpotLightEnabled(bool enabled);

/// <summary>
/// 今の「実効アクティブ」スポットライトが ON かどうか
/// </summary>
bool IsActiveSpotLightEnabled();


// ============================================================================
// AreaLight（Rect）
// ============================================================================

class AreaLightSource;

/// <summary>
/// エリアライト（矩形）を生成してハンドルを返す
/// </summary>
int CreateAreaLight(LightActivateMode activateMode = LightActivateMode::Add);

/// <summary>
/// エリアライト（矩形）を破棄する
/// </summary>
void DestroyAreaLight(int areaLightHandle);

/// <summary>
/// 互換：エリアライトを1個だけ有効化（Clear→Add）
/// </summary>
void SetActiveAreaLight(int areaLightHandle);

/// <summary>
/// 現在のアクティブエリアライトのハンドルを返す
/// </summary>
int GetActiveAreaLightHandle();

/// <summary>
/// アクティブエリアライト配列をクリアする
/// </summary>
void ClearActiveAreaLights();

/// <summary>
/// アクティブエリアライト配列に追加する
/// </summary>
bool AddActiveAreaLight(int areaLightHandle);

/// <summary>
/// アクティブエリアライト配列から削除する
/// </summary>
void RemoveActiveAreaLight(int areaLightHandle);

/// <summary>
/// アクティブエリアライトの数を取得する
/// </summary>
int GetActiveAreaLightCount();

/// <summary>
/// アクティブエリアライトのハンドルをインデックスで取得する
/// </summary>
int GetActiveAreaLightHandleAt(int index);

/// <summary>
/// エリアライトの実体ポインタを取得する
/// </summary>
AreaLightSource *GetAreaLightPtr(int areaLightHandle);

/// <summary>
/// エリアライトの ImGui 表示を行う
/// </summary>
void DrawImGuiAreaLight(int areaLightHandle, const char *name = nullptr);

void SetAreaLightEnabled(int areaLightHandle, bool enabled);
bool IsAreaLightEnabled(int areaLightHandle);

void SetActiveAreaLightEnabled(bool enabled);
bool IsActiveAreaLightEnabled();

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
// Primitive3D（デバッグ線）
// ===============================

/// <summary>
/// 3D線描画
/// </summary>
/// <param name="a">始点（ワールド座標）</param>
/// <param name="b">終点（ワールド座標）</param>
/// <param name="color">色（RGBA）</param>
/// <param name="depth">深度テストを行うか</param>
void DrawLine3D(const Vector3 &a, const Vector3 &b, const Vector4 &color,
                bool depth = true);

/// <summary>
/// 3DAABB描画
/// </summary>
/// <param name="mn">最小座標（ワールド座標）</param>
/// <param name="mx">最大座標（ワールド座標）</param>
/// <param name="color">色（RGBA）</param>
/// <param name="depth">深度テストを行うか</param>
void DrawAABB3D(const Vector3 &mn, const Vector3 &mx, const Vector4 &color,
                bool depth = true);

/// <summary>
/// 3Dグリッド - XZ描画
/// </summary>
/// <param name="halfSize">グリッドの半分のサイズ（例: 10 なら
/// -10..+10）</param>
/// <param name="step">グリッドの間隔</param>
/// <param name="color">色（RGBA）</param>
/// <param name="depth">深度テストを行うか</param>
void DrawGridXZ3D(int halfSize, float step, const Vector4 &color,
                  bool depth = true);

/// <summary>
/// 3Dグリッド - XY描画
/// </summary>
/// <param name="halfSize">グリッドの半分のサイズ（例: 10 なら
/// -10..+10）</param>
/// <param name="step">グリッドの間隔</param>
/// <param name="color">色（RGBA）</param>
/// <param name="depth">深度テストを行うか</param>
void DrawGridXY3D(int halfSize, float step, const Vector4 &color,
                  bool depth = true);

/// <summary>
/// 3Dグリッド - YZ描画
/// </summary>
/// <param name="halfSize">グリッドの半分のサイズ（例: 10 なら
/// -10..+10）</param>
/// <param name="step">グリッドの間隔</param>
/// <param name="color">色（RGBA）</param>
/// <param name="depth">深度テストを行うか</param>
void DrawGridYZ3D(int halfSize, float step, const Vector4 &color,
                  bool depth = true);

// 2Dパスを呼ばない時用（任意）
void FlushPrimitive3D();


// ===============================
// 画面オーバーレイ（ポスト風）
// ===============================

/// <summary>
/// 画面全体に白い霧（寒い雰囲気）をオーバーレイする
/// </summary>
/// <param name="timeSec">時間（秒）。ノイズを流すのに使用</param>
/// <param name="intensity">濃さ（0..1）</param>
/// <param name="scale">模様の大きさ（大きいほど細かい）</param>
/// <param name="speed">流れる速度（秒あたり）</param>
/// <param name="wind">流れる方向（UV空間）</param>
/// <param name="feather">もやの柔らかさ（0..0.5 くらい）</param>
/// <param name="bottomBias">下側を濃くする係数（0..1 くらい）</param>
/// <remarks>
/// - 3D/2D を描いた後、最後に呼ぶと "画面全体" にかかります。
/// - UI（ImGui等）まで霧を被せたくない場合は、UI描画の前に呼んでください。
/// </remarks>
void DrawFogOverlay(float timeSec, float intensity, float scale, float speed,
                    const Vector2 &wind, float feather, float bottomBias);

/// <summary>
/// FogOverlay の色（霧の色味）を設定する
/// </summary>
/// <param name="color">RGBA(0..1)。rgb=色、a=追加の濃さ倍率(基本は1)</param>
void SetFogOverlayColor(const Vector4 &color);

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
