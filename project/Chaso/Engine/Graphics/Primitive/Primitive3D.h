#pragma once
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode
#include "Math/Math.h"
#include "function/function.h" // CreateBufferResource

/// @class Primitive3D
/// @brief 3D 空間でのデバッグ用プリミティブ（ライン、グリッド、球、箱など）の描画を管理するクラス
/// @details フレームごとに頂点データを蓄積し、深度テストの有無を分けて一括描画します。
/// 各種デバッグ形状をピクセルやワールド座標で簡単に追加できるインターフェースを提供します。
class Primitive3D {
public:
  Primitive3D() = default;
  ~Primitive3D();

  /// @brief 初期化
  /// @param device D3D12 デバイス
  void Initialize(ID3D12Device *device);

  /// @brief 終了処理
  void Term();

  /// @brief フレームの開始を宣言し、ビュー・プロジェクション行列をセットする
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  /// @param blendAt3D 描画時のブレンドモード
  void BeginFrame(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj,
                  BlendMode blendAt3D = kBlendModeNone);

  /// @brief ラインを追加する
  /// @param a 開始点
  /// @param b 終了点
  /// @param color 描画色
  /// @param depth 深度テストを行うか
  void AddLine(const RC::Vector3 &a, const RC::Vector3 &b,
               const RC::Vector4 &color, bool depth = true);

  /// @brief AABB（軸平行境界ボックス）を追加する
  /// @param mn 最小座標
  /// @param mx 最大座標
  /// @param color 描画色
  /// @param depth 深度テストを行うか
  void AddAABB(const RC::Vector3 &mn, const RC::Vector3 &mx,
               const RC::Vector4 &color, bool depth = true);

  /// @brief XZ平面のグリッドを追加する
  /// @param halfSize 片側のサイズ（中心から端までの範囲）
  /// @param step グリッドの間隔
  /// @param color 描画色
  /// @param depth 深度テストを行うか
  void AddGridXZ(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  /// @brief XY平面のグリッドを追加する
  void AddGridXY(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  /// @brief YZ平面のグリッドを追加する
  void AddGridYZ(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  // --------------------------------------------------------------------------
  // 便利形状（デバッグ用）
  // --------------------------------------------------------------------------

  /// @brief ワイヤーフレーム球を追加する
  /// @param center 中心座標
  /// @param radius 半径
  /// @param color 描画色
  /// @param slices 経度分割数
  /// @param stacks 緯度分割数
  /// @param depth 深度テストを行うか
  void AddSphere(const RC::Vector3 &center, float radius,
                 const RC::Vector4 &color, int slices = 24, int stacks = 12,
                 bool depth = true);

  /// @brief 3本の軸リングのみで構成される軽量な球を追加する
  /// @param center 中心座標
  /// @param radius 半径
  /// @param color 描画色
  /// @param segments 各リングの分割数
  /// @param depth 深度テストを行うか
  void AddSphereRings(const RC::Vector3 &center, float radius,
                      const RC::Vector4 &color, int segments = 32,
                      bool depth = true);

  /// @brief 円弧を追加する
  /// @param center 中心座標
  /// @param normal 円弧が乗る平面の法線
  /// @param fromDir 開始方向（法線と直交するベクトル）
  /// @param radius 半径
  /// @param startRad 開始角 (ラジアン)
  /// @param endRad 終了角 (ラジアン)
  /// @param color 描画色
  /// @param segments 分割数
  /// @param depth 深度テストを行うか
  /// @param drawToCenter 中心から端点への線を引いて扇形にするか
  void AddArc(const RC::Vector3 &center, const RC::Vector3 &normal,
              const RC::Vector3 &fromDir, float radius, float startRad,
              float endRad, const RC::Vector4 &color, int segments = 32,
              bool depth = true, bool drawToCenter = false);

  /// @brief ワイヤーカプセルを追加する
  /// @param p0 線分の一方の端点（球の中心）
  /// @param p1 線分のもう一方の端点（球の中心）
  /// @param radius 半径
  /// @param color 描画色
  /// @param segments 分割数
  /// @param depth 深度テストを行うか
  void AddCapsule(const RC::Vector3 &p0, const RC::Vector3 &p1, float radius,
                  const RC::Vector4 &color, int segments = 16,
                  bool depth = true);

  /// @brief OBB（有向境界ボックス）を追加する
  /// @param center 中心座標
  /// @param axisX X軸方向（正規化ベクトル）
  /// @param axisY Y軸方向（正規化ベクトル）
  /// @param axisZ Z軸方向（正規化ベクトル）
  /// @param halfExtents 各軸方向の半サイズ
  /// @param color 描画色
  /// @param depth 深度テストを行うか
  void AddOBB(const RC::Vector3 &center, const RC::Vector3 &axisX,
              const RC::Vector3 &axisY, const RC::Vector3 &axisZ,
              const RC::Vector3 &halfExtents, const RC::Vector4 &color,
              bool depth = true);

  /// @brief 視錐台（コーナー8点指定）を追加する
  /// @param corners 頂点配列（0-3: Near 面 LB/LT/RT/RB, 4-7: Far 面 LB/LT/RT/RB）
  /// @param color 描画色
  /// @param depth 深度テストを行うか
  void AddFrustum(const RC::Vector3 corners[8], const RC::Vector4 &color,
                  bool depth = true);

  /// @brief カメラパラメータから視錐台を計算して追加する
  /// @param camPos カメラ位置
  /// @param forward 前方ベクトル
  /// @param up 上方ベクトル
  /// @param fovYRad 垂直視野角 (ラジアン)
  /// @param aspect アスペクト比 (W/H)
  /// @param nearZ 近クリップ面距離
  /// @param farZ 遠クリップ面距離
  /// @param color 描画色
  /// @param depth 深度テストを行うか
  void AddFrustumCamera(const RC::Vector3 &camPos, const RC::Vector3 &forward,
                        const RC::Vector3 &up, float fovYRad, float aspect,
                        float nearZ, float farZ, const RC::Vector4 &color,
                        bool depth = true);

  /// @brief 現在蓄積されている形状を描画する
  /// @param cl コマンドリスト
  /// @param depth 深度テストあり(true)か、なし(false)かのどちらを描画するか
  void Draw(ID3D12GraphicsCommandList *cl, bool depth);

  /// @brief 現在蓄積されている頂点データを GPU (頂点バッファ) に転送する
  void TransferVertices();

  /// @brief 転送済みの頂点バッファ内の特定範囲を直接描画する
  /// @param cl コマンドリスト
  /// @param depth 描画状態（深度テストあり/なし）
  /// @param start 開始頂点インデックス
  /// @param count 描画頂点数
  void DrawRange(ID3D12GraphicsCommandList *cl, bool depth, uint32_t start,
                 uint32_t count);

  /// @brief 現在蓄積されている頂点数を取得する
  /// @param depth true で深度テストあり、false で深度テストなしの頂点数
  /// @return 頂点数
  uint32_t GetVertexCount(bool depth) const;

  /// @brief 描画すべきデータが一つでも存在するか確認する
  /// @return 存在するなら true
  bool HasAny() const { return !vtxDepth_.empty() || !vtxNoDepth_.empty(); }

  /// @brief 設定されているブレンドモードを取得する
  /// @return ブレンドモード
  BlendMode BlendAt3D() const { return blendAt3D_; }

  /// @brief 蓄積された頂点データをクリアする（フレーム開始時などに使用）
  void Clear();

private:
  /// @struct Vertex
  /// @brief 3Dプリミティブ描画用の頂点構造体
  struct Vertex {
    RC::Vector3 pos;   ///< 位置座標
    RC::Vector4 color; ///< 頂点カラー
  };

  /// @struct PerFrameCB
  /// @brief フレームごとに転送する定数バッファ
  struct alignas(16) PerFrameCB {
    RC::Matrix4x4 View; ///< ビュー行列
    RC::Matrix4x4 Proj; ///< プロジェクション行列
  };

  static constexpr uint32_t kMaxDrawPerFrame = 64; ///< 1フレームあたりの最大描画呼び出し数
  static constexpr uint32_t Align256(uint32_t v) { return (v + 255u) & ~255u; }

  /// @brief 指定された頂点数を収容できるように頂点バッファを拡張/確保する
  void EnsureVB_(size_t vertexCount);

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  std::vector<Vertex> vtxDepth_;   ///< 深度テストありで描画する頂点リスト
  std::vector<Vertex> vtxNoDepth_; ///< 深度テストなしで描画する頂点リスト

  PerFrameCB perFrame_{};             ///< カレントフレームのカメラ行列
  BlendMode blendAt3D_ = kBlendModeNone; ///< 現在のブレンドモード

  // VB（動的拡張対応）
  Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
  Vertex *vbMap_ = nullptr;
  size_t vbCapacity_ = 0; ///< 頂点バッファの収容可能頂点数
  D3D12_VERTEX_BUFFER_VIEW vbView_{};

  // CB リングバッファ（行列転送用）
  Microsoft::WRL::ComPtr<ID3D12Resource> cbRes_;
  uint8_t *cbMap_ = nullptr;
  uint32_t cbStride_ = 0;
  uint32_t cbCursor_ = 0;
};
