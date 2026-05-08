#pragma once
#include "Particle.h"
#include "EffectPreset.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace RC {

/// <summary>
/// 全てのパーティクルシステムとエフェクトプリセットを一元管理するクラス
/// （シングルトンとして使用するか、Scene等に持たせる）
/// </summary>
class ParticleManager {
public:
    static ParticleManager& GetInstance() {
        static ParticleManager instance;
        return instance;
    }

    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    // 毎フレームの更新と描画
    void Update(const RC::Matrix4x4& view, const RC::Matrix4x4& proj);
    void Render(SceneContext& ctx, ID3D12GraphicsCommandList* cl);

    // デバッグ用UI
    void DrawImGui();

    // ============================================
    // システム（実体）の管理
    // ============================================
    
    // パーティクルシステムを登録する
    void RegisterSystem(const std::string& name, std::unique_ptr<Particle> system);

    // 名前でパーティクルシステムを取得する
    Particle* GetSystem(const std::string& name);

    // すべてのシステムを削除する（シーン切り替え時など）
    void ClearSystems();

    // ============================================
    // エフェクト（プリセット）の管理
    // ============================================

    // エフェクトのプリセットデータを登録する
    void RegisterPreset(const EffectPreset& preset);

    // 登録されたエフェクトを再生する
    void PlayEffect(const std::string& presetName, const Vector3& pos);

    // ============================================
    // 直接特定のパーティクルを放出するユーティリティ
    // ============================================
    void Emit(const std::string& particleName, const Vector3& pos, int count, float scale = 1.0f, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f});

private:
    ParticleManager() = default;
    ~ParticleManager() = default;

    std::unordered_map<std::string, std::unique_ptr<Particle>> systems_;
    std::unordered_map<std::string, EffectPreset> presets_;
};

} // namespace RC
