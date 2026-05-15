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

/// @class SafeUniformRealDistribution
/// @brief 最小値と最大値が逆転していても安全に動作する実数一様分布ヘルパー
template<typename T>
class SafeUniformRealDistribution {
    std::uniform_real_distribution<T> dist;
    T minVal, maxVal;
public:
    /// @brief コンストラクタ
    /// @param a 範囲の下限
    /// @param b 範囲の上限
    SafeUniformRealDistribution(T a, T b) : minVal(a), maxVal(b) {
        if (minVal < maxVal) {
            dist = std::uniform_real_distribution<T>(minVal, maxVal);
        } else {
            dist = std::uniform_real_distribution<T>(minVal, minVal + (T)0.0001);
        }
    }
    /// @brief 乱数を生成する
    /// @param eng 乱数エンジン
    /// @return 生成された値
    template<typename Engine>
    T operator()(Engine& eng) {
        if (minVal >= maxVal) return minVal;
        return dist(eng);
    }
};

/// @class Particle
/// @brief 全てのパーティクルシステムの基底クラス
/// @details CPU 側での粒子管理（移動、寿命、色変化）と、GPU へのインスタンシング転送（StructuredBuffer）を担当します。
/// 派生クラスで InitParticleCore や UpdateOneParticle をオーバーライドすることで、様々なエフェクトを実現できます。
class Particle {
public:
  /// @brief 初期化（標準の板ポリモデルを使用）
  /// @param ctx シーンコンテキスト
  virtual void Initialize(SceneContext &ctx);

  /// @brief 初期化（カスタムモデルを各粒子の形状として使用）
  /// @param ctx シーンコンテキスト
  /// @param model 使用するモデルデータ
  void InitializeWithModel(SceneContext &ctx, const ModelData &model);

  /// @brief 終了処理
  void Finalize();

  /// @brief デストラクタ
  virtual ~Particle() { Finalize(); }

  /// @brief フレーム更新処理
  /// @param view ビュー行列
  /// @param proj プロジェクション行列
  void Update(const RC::Matrix4x4 &view, const RC::Matrix4x4 &proj);

  /// @brief 描画処理（GPUへのデータ転送と DrawInstanced の発行）
  /// @param ctx シーンコンテキスト
  /// @param cl グラフィックスコマンドリスト
  void Render(SceneContext &ctx, ID3D12GraphicsCommandList *cl);

  /// @brief ImGui デバッグ UI を表示する
  void DrawImGui();

  /// @brief 新しいパーティクルを 1 つ生成する
  /// @param randomEngine 乱数生成器
  /// @param translate 生成座標
  /// @return 生成されたパーティクルデータ
  ParticleData MakeNewParticle(std::mt19937 &randomEngine,
                               const Vector3 &translate);

  /// @brief エミッタの設定に基づき、複数のパーティクルを一括放出する
  /// @param emitter エミッタ設定
  /// @param randomEngine 乱数生成器
  /// @return 生成されたパーティクルデータのリスト
  virtual std::vector<ParticleData> Emit(const Emitter &emitter,
                                         std::mt19937 &randomEngine);

  /// @brief 更新処理（位置や時間の進行）の有効無効を切り替える
  void SetEnableUpdate(bool enable);
  void ToggleEnableUpdate();
  bool IsEnableUpdate() const { return enableUpdate_; }

  /// @brief 加速度フィールド（風など）の使用有無を切り替える
  void SetUseAccelerationField(bool enable);
  void ToggleUseAccelerationField();
  bool IsUseAccelerationField() const { return useAccelerationField_; }

  /// @brief 最大数までパーティクルを即座に再生成する
  void RespawnAllMax();

  /// @brief 全てのパーティクルを削除する
  void ClearAll();

  /// @brief エミッタの座標を移動させる
  /// @param delta 移動量
  void MoveEmitter(const Vector3 &delta);

  /// @brief 現在のエミッタ設定に基づいてパーティクルを即座に追加する
  void AddParticlesFromEmitter();

  /// @brief エミッタによる自動生成（時間経過による放出）の有効無効を設定する
  void SetEmitterAutoSpawn(bool enable);

  /// @brief エミッタへの参照を取得する
  /// @return エミッタ
  Emitter &GetEmitter() { return emitter_; }
  const Emitter &GetEmitter() const { return emitter_; }

  /// @brief エミッタの座標を設定する
  void SetEmitterTranslation(const Vector3 &v) { emitter_.transform.translation = v; }
  /// @brief エミッタのグローバルカラーを設定する
  void SetEmitterColor(const Vector4 &c) { emitter_.globalColor = c; }
  /// @brief エミッタのグローバルスケールを設定する
  void SetEmitterScale(const Vector3 &s) { emitter_.globalScale = s; }

  /// @brief デルタタイムを設定する
  void SetDeltaTime(float dt) { deltaTime_ = dt; }

protected:
  /// @brief 使用するテクスチャパスを取得する（派生クラスでオーバーライド）
  virtual const char *GetTexturePath() const;

  /// @brief 個々のパーティクルの初期化ロジック（派生クラスでオーバーライド）
  virtual void InitParticleCore(ParticleData &particle,
                                std::mt19937 &randomEngine,
                                const Vector3 &emitterTranslate);

  /// @brief 個々のパーティクルのワールド行列を構築する
  virtual Matrix4x4 BuildWorldMatrix(const ParticleData &p,
                                     const Matrix4x4 &billboardMatrix) const;

  /// @brief フェード用アルファ値を計算する（デフォルトは線形フェードアウト）
  virtual float ComputeAlpha(const ParticleData &p) const;

  /// @brief 固定デルタタイムを取得する
  float GetDeltaTime() const { return deltaTime_; }

  /// @brief 個々のパーティクルの更新ロジック（派生クラスでオーバーライド）
  virtual void UpdateOneParticle(ParticleData &particle, float deltaTime);

  /// @brief 内部エミッタへの参照（派生クラス用）
  Emitter &EmitterRef() { return emitter_; }
  const Emitter &EmitterRef() const { return emitter_; }

  /// @brief ビルボード（常にカメラを向く挙動）の有無を設定する
  void SetUseBillboard(bool b) { useBillboard_ = b; }

  std::vector<ParticleData> particles; ///< パーティクル配列（CPU側）

private:
  void CommonInit_(SceneContext &ctx);
  void SetupVBFromModelData_(const ModelData &data);
  void TrimToMax_();

  const uint32_t kNumMaxInstance = 1000; ///< 最大同時表示数

  static constexpr uint32_t Align256(uint32_t s) { return (s + 255u) & ~255u; }

  // GPUリソース関連
  ModelData modelData{};        ///< 頂点データ
  uint32_t vertexCount_ = 0;    ///< 頂点数

  Microsoft::WRL::ComPtr<ID3D12Resource> vbResource_; ///< 頂点バッファ
  D3D12_VERTEX_BUFFER_VIEW vbView_{};               ///< VB ビュー

  ::StructuredBufferManager *sbMgr_ = nullptr;      ///< 構造化バッファ管理
  int instanceBufferId_ = -1;                       ///< インスタンスバッファID
  ParticleForGPU *instancingData_ = nullptr;        ///< GPU書き込み用マップポインタ
  D3D12_GPU_DESCRIPTOR_HANDLE instanceSrv_{};       ///< インスタンス用 SRV

  Microsoft::WRL::ComPtr<ID3D12Resource> cbMat_;    ///< マテリアル定数バッファ
  SpriteMaterial *cbMatMapped_ = nullptr;           ///< マテリアルマップポインタ
  D3D12_GPU_DESCRIPTOR_HANDLE textureSrv_{};        ///< テクスチャ SRV
  int texHandle_ = -1;                              ///< テクスチャハンドル

  Microsoft::WRL::ComPtr<ID3D12Device> device_;     ///< D3D12 デバイス

  uint32_t numInstance = 0; ///< 今フレームの生存インスタンス数

  Emitter emitter_{};                   ///< エミッタ設定
  AccelerationField accelerationField_; ///< 加速度フィールド

  bool IsCollision(const AABB &aabb, const Vector3 &point);

  bool visible_ = true;               ///< 表示フラグ
  bool enableUpdate_ = true;          ///< 更新フラグ
  bool useBillboard_ = true;          ///< ビルボードフラグ
  bool useAccelerationField_ = false; ///< 加速度フィールド使用フラグ
  bool useEmitterAutoSpawn_ = true;   ///< 自動放出フラグ

  RC::Vector2 uvScale_{1.0f, 1.0f};     ///< UV スケール
  RC::Vector2 uvTranslate_{0.0f, 0.0f}; ///< UV オフセット
  float uvRotate_ = 0.0f;               ///< UV 回転

  float deltaTime_ = 1.0f / 60.0f; ///< タイムステップ

  std::random_device seedGenerator;           ///< 乱数シード
  std::mt19937 randomEngine{seedGenerator()}; ///< 乱数エンジン

  BlendMode blendMode_ = kBlendModeAdd; ///< 合成モード
};
} // namespace RC
