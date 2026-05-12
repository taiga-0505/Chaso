#pragma once
#include "Sound.h"
#include <memory>
#include <string>

/// @brief BGMのグループ定義
enum class BgmGroup {
  Main = 0,     ///< メインBGM
  Clear = 1,    ///< クリアBGM
  GameOver = 2, ///< ゲームオーバーBGM
  Count         ///< 要素数
};

/// @brief BGMを管理するクラス
/// 複数のBGMグループを切り替えて再生する
class BgmManager {
public:
  /// @brief 初期化
  /// @param defaultVolume デフォルトの音量 (0.0 ~ 1.0)
  void Init(float defaultVolume = 1.0f) {
    EnsureSound_();
    SetMasterVolume(defaultVolume);
  }

  /// @brief BGMグループごとのファイルパスを登録
  /// @param g 登録先のBGMグループ
  /// @param path ファイルパス
  void SetPath(BgmGroup g, const char *path) {
    paths_[static_cast<int>(g)] = path ? path : "";
  }

  /// @brief 指定したグループのBGMを再生する。すでに同じBGMが再生中の場合は継続する。
  /// @param g 再生するBGMグループ
  /// @param loop ループ再生するかどうか
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

  /// @brief BGMを停止する
  void Stop() {
    EnsureSound_();
    sound_->Stop();
    current_ = BgmGroup::Count;
  }

  /// @brief マスター音量を設定する
  /// @param v 音量 (0.0 ~ 1.0)
  void SetMasterVolume(float v) {
    volume_ = v;
    if (sound_) {
      sound_->SetVolume(volume_);
    }
  }

  /// @brief 現在のマスター音量を取得する
  /// @return マスター音量
  float MasterVolume() const { return volume_; }

private:
  /// @brief Soundインスタンスの生成を保証する内部関数
  void EnsureSound_() {
    if (!sound_) {
      sound_ = std::make_unique<Sound>();
    }
  }

  std::unique_ptr<Sound> sound_; ///< BGM再生用のSoundインスタンス (1本を使い回す)
  std::string paths_[static_cast<int>(BgmGroup::Count)]; ///< 各グループのファイルパス
  BgmGroup current_ = BgmGroup::Count;                   ///< 現在再生中のグループ
  float volume_ = 1.0f;                                  ///< 現在の音量
};
