#pragma once
#include "Sound.h"
#include <memory>
#include <string>

// 使うBGMグループ
enum class BgmGroup { Main = 0, Clear = 1, GameOver = 2, Count };

class BgmManager {
public:
  void Init(float defaultVolume = 1.0f) {
    EnsureSound_();
    SetMasterVolume(defaultVolume);
  }

  // グループごとのファイルを登録
  void SetPath(BgmGroup g, const char *path) {
    paths_[static_cast<int>(g)] = path ? path : "";
  }

  // gのBGMが未再生なら再生／すでに同じなら何もしない（＝途切れない）
  void Ensure(BgmGroup g, bool loop = true) {
    if (sound_ && current_ == g && !paths_[static_cast<int>(g)].empty())
      return;
    EnsureSound_();
    sound_->Stop();
    current_ = g;
    const std::string &p = paths_[static_cast<int>(g)];
    if (!p.empty()) {
      sound_->Initialize(p.c_str());
      sound_->SetVolume(volume_);
      sound_->Play(loop);
    }
  }

  void Stop() {
    EnsureSound_();
    sound_->Stop();
    current_ = BgmGroup::Count;
  }

  void SetMasterVolume(float v) {
    volume_ = v;
    if (sound_) {
      sound_->SetVolume(volume_);
    }
  }
  float MasterVolume() const { return volume_; }

private:
  void EnsureSound_() {
    if (!sound_) {
      sound_ = std::make_unique<Sound>();
    }
  }

  std::unique_ptr<Sound> sound_; // 1本でOK（Main⇔Resultで差し替える）
  std::string paths_[static_cast<int>(BgmGroup::Count)];
  BgmGroup current_ = BgmGroup::Count;
  float volume_ = 1.0f;
};
