#pragma once
#include "GraphicsPipeline/GraphicsPipeline.h"
#include "Scene.h"
#include "struct.h"
#include <d3d12.h>
#include <list>
#include <random>
#include <wrl/client.h>

class StructuredBufferManager;

namespace RC {

// ==================
// パーティクル描画クラス
//  - 板ポリ1枚 + Instancing で複数描画
//  - CPU側でパーティクル配列を管理して、GPUにまとめて送る
// ==================
class Particle {
public:
  // 初期化 / 終了
  void Initialize(SceneContext &ctx);
  void Finalize();

  // 1フレーム分の更新 / 描画
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  // ImGui デバッグUI表示
  void DrawImGui();

  // 1個分のパーティクルをランダムに生成するヘルパー
  // （外からも使えそうなので一応 public のままにしておく）
  ParticleData MakeNewParticle(std::mt19937 &randomEngine,
                               const Vector3 &translate);

  std::list<ParticleData> Emit(const Emitter &emitter,
                               std::mt19937 &randomEngine);

  // ==============================
  // キー操作用のユーティリティ
  // ==============================
  // Update を止める / 動かす
  void SetEnableUpdate(bool enable);
  void ToggleEnableUpdate();
  bool IsEnableUpdate() const { return enableUpdate_; }

  // 風（加速度フィールド）ON/OFF
  void SetUseAccelerationField(bool enable);
  void ToggleUseAccelerationField();
  bool IsUseAccelerationField() const { return useAccelerationField_; }

  // 全部リスポーン（最大数で再生成）
  void RespawnAllMax();

  // 全部削除
  void ClearAll();

  // エミッタの位置を移動させる
  void MoveEmitter(const Vector3 &delta);

  // エミッタからパーティクルを追加
  void AddParticlesFromEmitter();

private:
  // ==================
  // 定数
  // ==================
  const uint32_t kNumMaxInstance = 100; // 同時に存在できるパーティクル数

  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }

  // ==================
  // GPUリソース関連
  // ==================

  // 板ポリ用モデル（頂点配列の入れ物）
  ModelData modelData{};
  uint32_t vertexCount_ = 0; // 板ポリの頂点数（6固定）

  // 頂点バッファ
  ID3D12Resource *vbResource_ = nullptr;
  D3D12_VERTEX_BUFFER_VIEW vbView_{};

  // Instancing 用 StructuredBuffer
  ::StructuredBufferManager *sbMgr_ = nullptr; // 非所有
  int instanceBufferId_ = -1;                  // SBManager 内のハンドル
  ParticleForGPU *instancingData_ = nullptr;   // Map した先頭ポインタ
  D3D12_GPU_DESCRIPTOR_HANDLE instanceSrv_{};  // VS から読む SRV (t0)

  // Material / Texture
  ID3D12Resource *cbMat_ = nullptr;          // SpriteMaterial 用 CB
  SpriteMaterial *cbMatMapped_ = nullptr;    // 一時的な Map 先
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{}; // パーティクル用テクスチャ

  // デバイス
  ID3D12Device *device_ = nullptr;

  // 今フレーム実際に描画するインスタンス数
  uint32_t numInstance = 0;

  // ==================
  // パーティクル状態（CPU側）
  // ==================
  std::list<ParticleData> particles;

  Emitter emitter_{};
  AccelerationField accelerationField_;

  bool IsCollision(const AABB &aabb, const Vector3 &point);

  // 表示／Update 制御フラグ
  bool visible_ = true;      // 描画するか
  bool enableUpdate_ = true; // 位置や時間を進めるか
  bool useBillboard_ = true; // カメラの方を向かせるか
  bool useAccelerationField_ = false; // 加速度を使うか

  // UV トランスフォーム（全パーティクル共通）
  RC::Vector2 uvScale_{1.0f, 1.0f};
  RC::Vector2 uvTranslate_{0.0f, 0.0f};
  float uvRotate_ = 0.0f;

  // 簡易的な固定Δt（とりあえず 1/60 固定）
  float deltaTime = 1.0f / 60.0f;

  // ランダム関連
  std::random_device seedGenerator;
  std::mt19937 randomEngine{seedGenerator()};

  BlendMode blendMode_ = kBlendModeAdd;
};

} // namespace RC
