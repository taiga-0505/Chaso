#pragma once

#include "Math/Math.h"
#include "Math/MathTypes.h"
#include <string>
#include <vector>
#include <map>

namespace RC {

struct KeyframeVector3 {
    float time;
    RC::Vector3 value;
};

struct KeyframeQuaternion {
    float time;
    RC::Quaternion value;
};

struct NodeAnimation {
    std::vector<KeyframeVector3> translate;
    std::vector<KeyframeQuaternion> rotate;
    std::vector<KeyframeVector3> scale;
};

struct Animation {
    float duration; // アニメーション全体の尺 (単位は秒)
    std::map<std::string, NodeAnimation> nodeAnimations;
};

/// @brief アニメーションファイルを読み込む
/// @param filePath ファイルパス
/// @return 解析されたAnimation構造体
Animation LoadAnimationFile(const std::string& filePath);

/// @brief Vector3のキーフレームから指定時刻の値を計算する
/// @param keyframes キーフレーム配列
/// @param time 時刻 (秒)
/// @return 補間されたVector3
Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);

/// @brief Quaternionのキーフレームから指定時刻の値を計算する
/// @param keyframes キーフレーム配列
/// @param time 時刻 (秒)
/// @return 補間されたQuaternion
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

} // namespace RC
