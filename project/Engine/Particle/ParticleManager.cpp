#include "ParticleManager.h"
#include <Common/Log/Log.h>

namespace RC {

void ParticleManager::Update(const RC::Matrix4x4& view, const RC::Matrix4x4& proj) {
    for (auto& pair : systems_) {
        if (pair.second) {
            pair.second->Update(view, proj);
        }
    }
}

void ParticleManager::Render(SceneContext& ctx, ID3D12GraphicsCommandList* cl) {
    for (auto& pair : systems_) {
        if (pair.second) {
            pair.second->Render(ctx, cl);
        }
    }
}

void ParticleManager::DrawImGui() {
#ifdef _DEBUG
    // 螳溯｣・・蠢・ｦ√↓蠢懊§縺ｦ諡｡蠑ｵ
#endif
}

void ParticleManager::RegisterSystem(const std::string& name, std::unique_ptr<Particle> system) {
    if (systems_.find(name) != systems_.end()) {
        Log::Print("ParticleManager: System '" + name + "' is already registered. Overwriting.");
    }
    systems_[name] = std::move(system);
}

Particle* ParticleManager::GetSystem(const std::string& name) {
    auto it = systems_.find(name);
    if (it != systems_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ParticleManager::ClearSystems() {
    systems_.clear();
    presets_.clear();
}

void ParticleManager::RegisterPreset(const EffectPreset& preset) {
    presets_[preset.name] = preset;
}

void ParticleManager::PlayEffect(const std::string& presetName, const Vector3& pos) {
    auto it = presets_.find(presetName);
    if (it == presets_.end()) {
        Log::Print("ParticleManager: Effect Preset '" + presetName + "' not found!");
        return;
    }

    const EffectPreset& preset = it->second;
    for (const auto& emitter : preset.emitters) {
        Vector3 emitPos;
        emitPos.x = pos.x + emitter.offset.x;
        emitPos.y = pos.y + emitter.offset.y;
        emitPos.z = pos.z + emitter.offset.z;
        Emit(emitter.particleName, emitPos, emitter.count, emitter.scale, emitter.color);
    }
}

void ParticleManager::Emit(const std::string& particleName, const Vector3& pos, int count, float scale, const Vector4& color) {
    Particle* system = GetSystem(particleName);
    if (!system) {
        Log::Print("ParticleManager: Emit failed. System '" + particleName + "' not found.");
        return;
    }

    system->SetEmitterTranslation(pos);
    system->SetEmitterScale({scale, scale, scale});
    system->SetEmitterColor(color);
    system->GetEmitter().count = count;
    system->AddParticlesFromEmitter();
}

} // namespace RC
