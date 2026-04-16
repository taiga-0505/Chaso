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
  void InitializeWithModel(SceneContext &ctx, const ModelData &model);
  void Finalize();
  ~Particle() { Finalize(); }

  // 1フレーム分の更新 / 描画
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  // ImGui デバッグUI表示
  void DrawImGui();

  // 1個分のパーティクルをランダムに生成するヘルパー
  // （外からも使えそうなので一応 public のままにしておく）
  ParticleData MakeNewParticle(std::mt19937 &randomEngine,
                               const Vector3 &translate);

  virtual std::vector<ParticleData> Emit(const Emitter &emitter,
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

  // emitter の自動生成を ON/OFF
  void SetEmitterAutoSpawn(bool enable);

  // エミッタへのアクセサ
  Emitter &GetEmitter() { return emitter_; }
  const Emitter &GetEmitter() const { return emitter_; }

  void SetEmitterTranslation(const Vector3 &v) { emitter_.transform.translation = v; }
  void SetEmitterColor(const Vector4 &c) { emitter_.globalColor = c; }
  void SetEmitterScale(const Vector3 &s) { emitter_.globalScale = s; }

protected:
  // ==============================
  // エフェクトごとに差し替え可能なフック
  // ==============================

  // 使用するテクスチャのパス
  //  派生クラスでオーバーライドして差し替え可能
  virtual const char *GetTexturePath() const;

  // 1個分のパーティクル初期化
  //  位置・速度・色・寿命などを決める処理
  virtual void InitParticleCore(ParticleData &particle,
                                std::mt19937 &randomEngine,
                                const Vector3 &emitterTranslate);

  // 1個分のワールド行列を作る
  virtual Matrix4x4 BuildWorldMatrix(const ParticleData &p,
                                     const Matrix4x4 &billboardMatrix) const;

  // フェード用アルファ(0〜1)（デフォルトは 1 - current/life）
  virtual float ComputeAlpha(const ParticleData &p) const;

  // 派生クラスが「Particle基盤の固定Δt(1/60)」を使えるようにする
  float GetDeltaTime() const { return deltaTime; }

  // 1個分のパーティクル更新
  //  位置や回転、色などを進める処理
  virtual void UpdateOneParticle(ParticleData &particle, float deltaTime);

  Emitter &EmitterRef() { return emitter_; }
  const Emitter &EmitterRef() const { return emitter_; }

  std::vector<ParticleData> particles;

private:
  void CommonInit_(SceneContext &ctx);
  void SetupVBFromModelData_(const ModelData &data);
  void TrimToMax_();

  // ==================
  // 定数
  // ==================
  const uint32_t kNumMaxInstance = 1000; // 同時に存在できるパーティクル数

  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }

  // ==================
  // GPUリソース関連
  // ==================

  // 板ポリ用モデル（頂点配列の入れ物）
  ModelData modelData{};
  uint32_t vertexCount_ = 0; // 板ポリの頂点数（6固定）

  // 頂点バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> vbResource_;
  D3D12_VERTEX_BUFFER_VIEW vbView_{};

  // Instancing 用 StructuredBuffer
  ::StructuredBufferManager *sbMgr_ = nullptr; // 非所有
  int instanceBufferId_ = -1;                  // SBManager 内のハンドル
  ParticleForGPU *instancingData_ = nullptr;   // Map した先頭ポインタ
  D3D12_GPU_DESCRIPTOR_HANDLE instanceSrv_{};  // VS から読む SRV (t0)

  // Material / Texture
  Microsoft::WRL::ComPtr<ID3D12Resource> cbMat_; // SpriteMaterial 用 CB
  SpriteMaterial *cbMatMapped_ = nullptr;        // 一時的な Map 先
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{}; // パーティクル用テクスチャ
  int texHandle_ = -1;

  // デバイス
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  // 今フレーム実際に描画するインスタンス数
  uint32_t numInstance = 0;

  // ==================
  // パーティクル状態（CPU側）
  // ==================

  Emitter emitter_{};
  AccelerationField accelerationField_;

  bool IsCollision(const AABB &aabb, const Vector3 &point);

  // 表示／Update 制御フラグ
  bool visible_ = true;               // 描画するか
  bool enableUpdate_ = true;          // 位置や時間を進めるか
  bool useBillboard_ = true;          // カメラの方を向かせるか
  bool useAccelerationField_ = false; // 加速度を使うか
  bool useEmitterAutoSpawn_ = true;   // エミッタの自動スポーンを使うか

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
