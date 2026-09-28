#pragma once
#include "IComponent.h"
#include "TransformComponent.h"
#include "Audio/AudioEngine.h"
#include "Math/MathTypes.h"
#include <cmath>
#include <nlohmann/json.hpp>

/// @brief 3D 音響の聞き手（Unity の AudioListener 相当）
/// @details 付けなくても 3D 音響は動く（描画中のカメラが聞き手になる）。
///          「距離はプレイヤー基準で測りたい」ときにプレイヤー等のエンティティへ付ける。
///          シーン内で有効なものが複数あっても、最初に見つかった 1 つだけが使われる。
///
///          - 位置 … このエンティティの Transform（ワールド座標）
///          - 向き … orientToCamera が true（既定）ならカメラの向き。false なら Transform の +Z を正面とする
///                    （三人称視点でプレイヤーに付ける場合、向きだけカメラに合わせると
///                     画面の左右とスピーカーの左右が一致して違和感がない）
///          - 速度 … 位置の差分から自動で推定（ドップラー効果用）
///
///          更新は DataDrivenScene::UpdateAudio() が毎フレーム Tick() を呼んで行う。
class AudioListenerComponent : public IComponent {
public:
  bool orientToCamera = true; ///< 向きをカメラに合わせる（位置だけこのエンティティ）

  const char *TypeName() const override { return "AudioListenerComponent"; }

  nlohmann::json Serialize() const override {
    return {
        {"orientToCamera", orientToCamera},
    };
  }

  void Deserialize(const nlohmann::json &j) override {
    orientToCamera = j.value("orientToCamera", true);
    ResetMotion();
  }

  /// @brief 毎フレームの更新。姿勢を求めて AudioEngine のリスナーに設定する
  /// @param tr このエンティティの Transform
  /// @param cameraView 描画中のカメラのビュー行列（orientToCamera 用。nullptr なら Transform の向きを使う）
  /// @param playing 再生中か。false（編集モード）のときは速度を 0 にする（ギズモ操作でドップラーがかからないように）
  /// @param dt 前フレームからの経過時間（秒）
  void Tick(const TransformComponent &tr, const RC::Matrix4x4 *cameraView, bool playing, float dt) {
    AudioListenerPose pose = AudioEngine::PoseFromWorldMatrix(tr.GetWorldMatrix());
    if (orientToCamera && cameraView) {
      const AudioListenerPose cam = AudioEngine::PoseFromViewMatrix(*cameraView);
      pose.forward = cam.forward;
      pose.up = cam.up;
    }

    // 速度: 位置の差分。1 フレームで音速の半分以上動いたらテレポートとみなして 0
    pose.velocity = {0.0f, 0.0f, 0.0f};
    if (playing && hasPrevPos_ && dt > 0.0f) {
      const RC::Vector3 v = {(pose.position.x - prevPos_.x) / dt, (pose.position.y - prevPos_.y) / dt,
                             (pose.position.z - prevPos_.z) / dt};
      const float speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
      if (std::isfinite(speed) && speed <= AudioEngine::Get().GetSpeedOfSound() * 0.5f) {
        pose.velocity = v;
      }
    }
    prevPos_ = pose.position;
    hasPrevPos_ = true;
    lastPose_ = pose;

    AudioEngine::Get().SetListener(pose);
  }

  /// @brief 速度推定の履歴を捨てる（非アクティブになったとき等）
  void ResetMotion() {
    hasPrevPos_ = false;
    lastPose_.velocity = {0.0f, 0.0f, 0.0f};
  }

  /// @brief 最後にエンジンへ渡した姿勢（Inspector 表示用）
  const AudioListenerPose &LastPose() const { return lastPose_; }

private:
  RC::Vector3 prevPos_{0.0f, 0.0f, 0.0f}; ///< 前フレームの位置（速度推定用）
  bool hasPrevPos_ = false;               ///< prevPos_ が有効か
  AudioListenerPose lastPose_{};          ///< 最後に設定した姿勢
};
