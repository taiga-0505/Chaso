#pragma once
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>

#include "GraphicsPipeline/GraphicsPipeline.h" // BlendMode
#include "Math/Math.h"
#include "function/function.h" // CreateBufferResource

class Primitive3D {
public:
  Primitive3D() = default;
  ~Primitive3D();

  void Initialize(ID3D12Device *device);
  void Term();

  // 3Dパス開始（PreDraw3D から呼ぶ想定）
  void BeginFrame(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj,
                  BlendMode blendAt3D = kBlendModeNone);

  void AddLine(const RC::Vector3 &a, const RC::Vector3 &b,
               const RC::Vector4 &color, bool depth = true);

  void AddAABB(const RC::Vector3 &mn, const RC::Vector3 &mx,
               const RC::Vector4 &color, bool depth = true);

  void AddGridXZ(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  void AddGridXY(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  void AddGridYZ(int halfSize, float step, const RC::Vector4 &color,
                 bool depth = true);

  // --------------------------------------------------------------------------
  // 便利形状（デバッグ用）
  // --------------------------------------------------------------------------

  // ワイヤーフレーム球（緯線/経線）
  // slices: 経度分割（周方向）
  // stacks: 緯度分割（上下方向）
  void AddSphere(const RC::Vector3 &center, float radius,
                 const RC::Vector4 &color, int slices = 24, int stacks = 12,
                 bool depth = true);

  // 3本リングだけの軽量球（XY/XZ/YZ）
  void AddSphereRings(const RC::Vector3 &center, float radius,
                      const RC::Vector4 &color, int segments = 32,
                      bool depth = true);

  // 3Dアーク（円弧）
  // - normal : 円弧の面の法線（ワールド）
  // - fromDir: 円弧開始方向（normal と直交する成分が使われる）
  // - start/end: ラジアン
  // - drawToCenter: true なら扇形の "中心→両端" の線も引く
  void AddArc(const RC::Vector3 &center, const RC::Vector3 &normal,
              const RC::Vector3 &fromDir, float radius, float startRad,
              float endRad, const RC::Vector4 &color, int segments = 32,
              bool depth = true, bool drawToCenter = false);

  // ワイヤーカプセル（p0/p1 が端の球の中心）
  void AddCapsule(const RC::Vector3 &p0, const RC::Vector3 &p1, float radius,
                  const RC::Vector4 &color, int segments = 16,
                  bool depth = true);

  // OBB（回転付き箱）
  // - axisX/Y/Z: 箱のローカル軸（正規化済みが望ましい）
  // - halfExtents: 各軸方向の半サイズ
  void AddOBB(const RC::Vector3 &center, const RC::Vector3 &axisX,
              const RC::Vector3 &axisY, const RC::Vector3 &axisZ,
              const RC::Vector3 &halfExtents, const RC::Vector4 &color,
              bool depth = true);

  // 視錐台（コーナー8点）
  // corners[0..3] = near plane, corners[4..7] = far plane
  // 推奨順:
  //  0: nearLB, 1: nearLT, 2: nearRT, 3: nearRB
  //  4: farLB,  5: farLT,  6: farRT,  7: farRB
  void AddFrustum(const RC::Vector3 corners[8], const RC::Vector4 &color,
                  bool depth = true);

  // 視錐台（カメラパラメータから生成）
  // forward/up はカメラの向き（ワールド）
  void AddFrustumCamera(const RC::Vector3 &camPos, const RC::Vector3 &forward,
                        const RC::Vector3 &up, float fovYRad, float aspect,
                        float nearZ, float farZ, const RC::Vector4 &color,
                        bool depth = true);


  // RenderCommon 側で PSO/Root はセット済み前提
  void Draw(ID3D12GraphicsCommandList *cl, bool depth);

  /// <summary>
  /// 現在蓄積されている頂点データをすべて GPU (VB) に転送する
  /// </summary>
  void TransferVertices();

  /// <summary>
  /// 転送済みの VB 内の特定範囲を描画する
  /// </summary>
  void DrawRange(ID3D12GraphicsCommandList *cl, bool depth, uint32_t start,
                 uint32_t count);

  /// <summary>
  /// 現在蓄積されている頂点数を取得する
  /// </summary>
  uint32_t GetVertexCount(bool depth) const;

  bool HasAny() const { return !vtxDepth_.empty() || !vtxNoDepth_.empty(); }
  BlendMode BlendAt3D() const { return blendAt3D_; }

  void Clear();


private:
  struct Vertex {
    RC::Vector3 pos;
    RC::Vector4 color;
  };

  struct alignas(16) PerFrameCB {
    RC::Matrix4x4 View;
    RC::Matrix4x4 Proj;
  };

  static constexpr uint32_t kMaxDrawPerFrame = 64;
  static constexpr uint32_t Align256(uint32_t v) { return (v + 255u) & ~255u; }

  void EnsureVB_(size_t vertexCount);

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  // vertices
  std::vector<Vertex> vtxDepth_;
  std::vector<Vertex> vtxNoDepth_;

  // view/proj for this frame
  PerFrameCB perFrame_{};
  BlendMode blendAt3D_ = kBlendModeNone;

  // VB
  Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
  Vertex *vbMap_ = nullptr;
  size_t vbCapacity_ = 0; // vertex count
  D3D12_VERTEX_BUFFER_VIEW vbView_{};

  // CB ring
  Microsoft::WRL::ComPtr<ID3D12Resource> cbRes_;
  uint8_t *cbMap_ = nullptr;
  uint32_t cbStride_ = 0;
  uint32_t cbCursor_ = 0;
};
