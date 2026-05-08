#pragma once
#include <Math/Math.h>
#include <string>
#include <vector>

namespace RC {

/// <summary>
/// 1種類のパーティクルをどのように放出するか（エミッター）の設定
/// </summary>
struct EffectEmitterDesc {
    std::string particleName;           // 使用するパーティクル名（例: "Flash", "Ring"）
    int count = 1;                      // 放出数
    float scale = 1.0f;                 // スケール倍率
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 乗算カラー
    Vector3 offset = {0.0f, 0.0f, 0.0f};      // 発生位置からのオフセット
};

/// <summary>
/// 複数のパーティクルを組み合わせたエフェクトのプリセットデータ
/// （将来的にはJSONから読み込む）
/// </summary>
struct EffectPreset {
    std::string name; // エフェクト名（例: "Hit", "Explosion"）
    std::vector<EffectEmitterDesc> emitters;

    // ヘルパー：エミッターを追加する
    void AddEmitter(const std::string& pName, int pCount, float pScale = 1.0f, 
                    const Vector4& pColor = {1.0f, 1.0f, 1.0f, 1.0f}, 
                    const Vector3& pOffset = {0.0f, 0.0f, 0.0f}) {
        emitters.push_back({pName, pCount, pScale, pColor, pOffset});
    }
};

} // namespace RC
