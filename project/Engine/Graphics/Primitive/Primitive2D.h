#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

#include "Math/Math.h"
#include "function/function.h" // CreateBufferResource
#include "struct.h"

/// @class Primitive2D
/// @brief 画面空間（2D）での簡易図形描画を管理するクラス
/// @details 線、矩形、円、三角形などのプリミティブ図形を、ピクセル座標指定で描画できます。
/// 内部でリングバッファ形式の定数バッファを管理し、1フレームに複数の図形を効率的に描画します。
class Primitive2D {
public:
  Primitive2D() = default;
  ~Primitive2D();

  /// @brief 初期化
  /// @param device D3D12 デバイス
  /// @param screenW 初期画面幅
  /// @param screenH 初期画面高さ
  void Initialize(ID3D12Device *device, float screenW, float screenH);

  /// @brief 描画先のスクリーンサイズを更新する
  /// @param w 幅
  /// @param h 高さ
  void SetScreenSize(float w, float h);

  /// @brief 描画に使用するテクスチャ（スプライト用など）を設定する
  /// @param srv テクスチャの GPU ハンドル
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) { srv_ = srv; }

  // ---- shape setters ----

  /// @brief 線を描画する
  /// @param p0 開始座標 (ピクセル)
  /// @param p1 終了座標 (ピクセル)
  /// @param thickness 線の太さ
  /// @param color 描画色
  /// @param feather 境界のぼかし強度（1.0 で通常）
  void SetLine(const RC::Vector2 &p0, const RC::Vector2 &p1, float thickness,
               const RC::Vector4 &color, float feather = 1.0f);

  /// @brief 矩形を描画する
  /// @param mn 左上座標
  /// @param mx 右下座標
  /// @param fillMode 塗りつぶしモード（Fill / Stroke）
  /// @param thickness Stroke 時の線の太さ
  /// @param color 描画色
  /// @param feather 境界のぼかし強度
  void SetRect(const RC::Vector2 &mn, const RC::Vector2 &mx,
               kFillMode fillMode, float thickness,
               const RC::Vector4 &color, float feather = 1.0f);

  /// @brief 円を描画する
  /// @param c 中心座標
  /// @param r 半径
  /// @param fillMode 塗りつぶしモード
  /// @param thickness Stroke 時の線の太さ
  /// @param color 描画色
  /// @param feather 境界のぼかし強度
  void SetCircle(const RC::Vector2 &c, float r, kFillMode fillMode,
                 float thickness, const RC::Vector4 &color,
                 float feather = 1.0f);

  /// @brief 三角形を描画する
  /// @param p0 頂点0
  /// @param p1 頂点1
  /// @param p2 頂点2
  /// @param fillMode 塗りつぶしモード
  /// @param thickness Stroke 時の線の太さ
  /// @param color 描画色
  /// @param feather 境界のぼかし強度
  void SetTriangle(const RC::Vector2 &p0, const RC::Vector2 &p1,
                   const RC::Vector2 &p2, kFillMode fillMode,
                   float thickness, const RC::Vector4 &color,
                   float feather = 1.0f);

  /// @brief スプライト（テクスチャ付き矩形）を描画する
  /// @param mn 左上座標
  /// @param mx 右下座標
  /// @param color カラー乗算値
  /// @param uvMin UV左上
  /// @param uvMax UV右下
  void SetSpriteRect(const RC::Vector2 &mn, const RC::Vector2 &mx,
                     const RC::Vector4 &color,
                     const RC::Vector2 &uvMin = {0, 0},
                     const RC::Vector2 &uvMax = {1, 1});

  /// @brief フレームの開始を宣言し、バッファカーソルをリセットする
  void BeginFrame(); 

  /// @brief 描画を実行する
  /// @param cmdList コマンドリスト
  void Draw(ID3D12GraphicsCommandList *cmdList);

private:
  /// @brief リソースの解放
  void Release();

public:
  /// @struct Vertex
  /// @brief 2D描画用の頂点構造体
  struct Vertex {
    RC::Vector4 pos; ///< 座標 (クリップ空間)
    RC::Vector2 uv;  ///< テクスチャ座標
  };

private:
  /// @struct VB
  /// @brief 頂点バッファリソース管理用
  struct VB {
    Microsoft::WRL::ComPtr<ID3D12Resource> res;
    D3D12_VERTEX_BUFFER_VIEW view{};
  };

  /// @struct CB
  /// @brief 定数バッファリソース管理用テンプレート
  template <class T> struct CB {
    Microsoft::WRL::ComPtr<ID3D12Resource> res;
    T *map = nullptr;
  };

  struct UInt4 {
    uint32_t x, y, z, w;
  };

  /// @struct Params
  /// @brief シェーダーに送る図形パラメータ構造体（16バイトアライメント必須）
  struct alignas(16) Params {
    RC::Vector4 A;       ///< 座標パラメータA
    RC::Vector4 B;       ///< 座標パラメータB
    RC::Vector4 C;       ///< 座標パラメータC
    RC::Vector4 D;       ///< 座標パラメータD
    UInt4 U;             ///< 各種フラグ（形状、塗りつぶし、太さなど）
    RC::Vector4 Color;   ///< カラー
    RC::Vector4 UVRect;  ///< スプライト用UV範囲
  };

  struct alignas(16) DummyVS {
    RC::Matrix4x4 World;
    RC::Matrix4x4 WVP;
  };

  /// @enum ShapeType
  /// @brief 形状識別用の定数
  enum : uint32_t {
    SHAPE_LINE = 0,
    SHAPE_RECT = 1,
    SHAPE_CIRCLE = 2,
    SHAPE_SPRITE = 3,
    SHAPE_TRIANGLE = 4
  };
  
  /// @enum Flags
  /// @brief 描画オプションフラグ
  enum : uint32_t { FLAG_STROKE = 1, FLAG_TEX = 2 };

  static constexpr uint32_t kMaxDrawPerFrame = 256; ///< 1フレームあたりの最大描画数
  static constexpr uint32_t Align256(uint32_t v) { return (v + 255u) & ~255u; }

  Params paramsCPU_{}; ///< CPU側のパラメータキャッシュ

  // リング化したCB（一括描画用）
  Microsoft::WRL::ComPtr<ID3D12Resource> cbParamsRes_;
  uint8_t *cbParamsMap_ = nullptr;
  uint32_t cbStride_ = 0;
  uint32_t cbCursor_ = 0;

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  float screenW_ = 0.0f;
  float screenH_ = 0.0f;

  VB vb_{};
  CB<Params> cbParams_{};
  CB<DummyVS> cbDummy_{};

  D3D12_GPU_DESCRIPTOR_HANDLE srv_{}; ///< 現在バインドされている SRV
  bool visible_ = true;              ///< 可視フラグ
};
