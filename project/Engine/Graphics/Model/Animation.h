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
    // 解析に失敗した場合はここが 0 のまま返るため、必ず初期化しておくこと
    float duration = 0.0f; // アニメーション全体の尺 (単位は秒)
    std::map<std::string, NodeAnimation> nodeAnimations;
};

/// @brief アニメーションファイルを読み込む（最初のアニメーションを使用）
/// @param filePath ファイルパス
/// @return 解析されたAnimation構造体
/// @note 結果は (パス, インデックス) をキーにキャッシュされる。詳細は下記オーバーロードを参照
Animation LoadAnimationFile(const std::string& filePath);

/// @brief アニメーションファイルからインデックス指定でアニメーションを読み込む
/// @param filePath ファイルパス
/// @param animIndex アニメーションインデックス（0始まり）
/// @return 解析されたAnimation構造体
/// @details 解析結果は (パス, インデックス) をキーにキャッシュされる。
///          2 回目以降は assimp によるファイル読み込みが起きないため、
///          「歩き→攻撃」のようなクリップ切り替えを毎フレーム判定しても
///          ディスクアクセスによるヒッチが出ない。
///          解析に失敗した場合も空の結果をキャッシュする（毎回リトライさせないため）。
/// @note スレッドセーフではない。描画スレッドからのみ呼ぶこと。
Animation LoadAnimationFile(const std::string& filePath, int animIndex);

/// @brief アニメーションファイル内のアニメーション数を取得する
/// @param filePath ファイルパス
/// @return アニメーション数（ファイルが無効なら 0）
/// @note 結果はパスをキーにキャッシュされる
int GetAnimationCount(const std::string& filePath);

/// @brief アニメーションのキャッシュを全て破棄する
/// @details ファイルを差し替えて再読み込みさせたい場合に呼ぶ。
///          常駐させたくない場合はシーン遷移時に呼んでもよいが、
///          遷移のたびに再パースが走るため通常は保持したままでよい。
void ClearAnimationCache();

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
