#pragma once
#include <Math/Math.h>
#include <string>
#include <vector>

namespace RC {

/// @struct EffectEmitterDesc
/// @brief 1種類のパーティクルの放出設定（エミッター）を保持する構造体
struct EffectEmitterDesc {
    std::string particleName;           ///< 使用するパーティクルの種類名（例: "Flash", "Ring"）
    int count = 1;                      ///< 一度に放出する数
    float scale = 1.0f;                 ///< パーティクルのスケール倍率
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; ///< パーティクルへの乗算カラー (RGBA)
    Vector3 offset = {0.0f, 0.0f, 0.0f};      ///< 発生座標からの相対オフセット
};

/// @struct EffectPreset
/// @brief 複数のパーティクルエミッターを組み合わせたエフェクトのプリセットデータ
/// @details 一つのヒット演出や爆発演出を、複数の異なるパーティクル設定を組み合わせて定義します。
struct EffectPreset {
    std::string name; ///< エフェクト名（例: "Hit", "Explosion"）
    std::vector<EffectEmitterDesc> emitters; ///< このエフェクトに含まれるエミッターのリスト

    /// @brief エミッター設定をプリセットに追加するヘルパーメソッド
    /// @param pName パーティクル種類名
    /// @param pCount 放出数
    /// @param pScale スケール
    /// @param pColor カラー
    /// @param pOffset オフセット
    void AddEmitter(const std::string& pName, int pCount, float pScale = 1.0f, 
                    const Vector4& pColor = {1.0f, 1.0f, 1.0f, 1.0f}, 
                    const Vector3& pOffset = {0.0f, 0.0f, 0.0f}) {
        emitters.push_back({pName, pCount, pScale, pColor, pOffset});
    }
};

} // namespace RC
