#pragma once
#include "Sound.h" // SoundLoadAudio / SoundUnload / SoundData
#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>
#include <xaudio2.h>

using Microsoft::WRL::ComPtr;
#pragma comment(lib, "xaudio2.lib")

/// @brief 効果音(SE)の識別子
enum class SeId {
  Cursor = 0,  ///< カーソル移動音
  Decide = 1,  ///< 決定音
  Cancel = 2,  ///< キャンセル音
  Hit = 3,     ///< ヒット音
  Damage = 4,  ///< ダメージ音
  Clear = 5,   ///< クリア音
  Count        ///< 要素数
};

/// @brief 効果音(SE)を管理するクラス
/// XAudio2を使用して複数のSEを並列再生・管理する
class SeManager {
public:
  /// @brief 初期化
  /// @param defaultVolume デフォルトの音量 (0.0 ~ 1.0)
  void Init(float defaultVolume = 1.0f) {
    HRESULT hr = XAudio2Create(&xaudio_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));

    IXAudio2MasteringVoice *rawMaster = nullptr;
    hr = xaudio_->CreateMasteringVoice(&rawMaster);
    assert(SUCCEEDED(hr));
    master_.reset(rawMaster);

    volume_ = defaultVolume;
  }

  /// @brief 終了処理。再生中のSEを停止し、ロード済みのリソースを解放する。
  void Term() {
    StopAll();
    // キャッシュ解放
    for (auto &kv : entries_) {
      if (kv.second.loaded) {
        SoundUnload(&kv.second.data);
      }
    }
    entries_.clear();
    master_.reset();
    xaudio_.Reset();
  }

  /// @brief SEのファイルパスを登録する。あとから差し替えも可能。
  /// @param id 登録するSEの識別子
  /// @param path ファイルパス
  void SetPath(SeId id, const char *path) {
    const int k = (int)id;
    auto &e = entries_[k];
    // 既存ロード済みなら解放して差し替え
    if (e.loaded) {
      SoundUnload(&e.data);
      e = {};
    }
    e.path = path ? path : "";
    e.loaded = false;
  }

  /// @brief 指定したIDのSEを再生する。
  /// @param id 再生するSEの識別子
  /// @param volScale 個別音量の倍率 (デフォルト1.0)
  void Play(SeId id, float volScale = 1.0f) {
    const int k = (int)id;
    auto it = entries_.find(k);
    if (it == entries_.end() || it->second.path.empty())
      return;

    auto &e = it->second;
    if (!e.loaded) {
      e.data = SoundLoadAudio(e.path.c_str()); // mp3/wav両対応
      e.loaded = true;
    }
    IXAudio2SourceVoice *raw = nullptr;
    HRESULT hr = xaudio_->CreateSourceVoice(&raw, &e.data.wfex);
    if (FAILED(hr) || !raw)
      return;

    SourceVoicePtr v{raw};
    const float vol = (std::max)(0.0f, volume_ * volScale);
    v->SetVolume(vol);

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = e.data.pBuffer.get();
    buf.AudioBytes = e.data.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;
    buf.LoopCount = 0; // SEは基本ループしない
    hr = v->SubmitSourceBuffer(&buf);
    if (FAILED(hr)) {
      return;
    }

    v->Start();
    ActiveVoice av;
    av.v = std::move(v);
    av.slot = k;
    av.volScale = volScale;
    actives_.push_back(std::move(av));
  }

  /// @brief 鳴り終わったVoiceの破棄など、毎フレームの更新処理。
  void Update() {
    // 後ろから消す
    for (int i = (int)actives_.size() - 1; i >= 0; --i) {
      auto *v = actives_[i].v.get();
      XAUDIO2_VOICE_STATE st{};
      v->GetState(&st);
      if (st.BuffersQueued == 0) {
        actives_.erase(actives_.begin() + i);
      }
    }
  }

  /// @brief マスター音量を設定する。再生中のSEにも即座に反映される。
  /// @param v 音量 (0.0 ~ 1.0)
  void SetMasterVolume(float v) {
    volume_ = (std::max)(0.0f, v);
    // 再生中にも反映
    for (auto &av : actives_) {
      if (av.v)
        av.v->SetVolume(volume_ * av.volScale);
    }
  }

  /// @brief 現在のマスター音量を取得する。
  /// @return マスター音量
  float MasterVolume() const { return volume_; }

  /// @brief 全てのSE再生を停止する。
  void StopAll() { actives_.clear(); }

private:
  /// @brief SourceVoiceのカスタムデリータ
  struct SourceVoiceDeleter {
    void operator()(IXAudio2SourceVoice *v) const noexcept {
      if (v) {
        v->Stop(0);
        v->DestroyVoice();
      }
    }
  };
  /// @brief MasterVoiceのカスタムデリータ
  struct MasterVoiceDeleter {
    void operator()(IXAudio2MasteringVoice *v) const noexcept {
      if (v) {
        v->DestroyVoice();
      }
    }
  };
  using SourceVoicePtr =
      std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>;
  using MasterVoicePtr =
      std::unique_ptr<IXAudio2MasteringVoice, MasterVoiceDeleter>;

  /// @brief SEリソースの管理用エントリ
  struct Entry {
    std::string path;    ///< ファイルパス
    SoundData data{};    ///< PCMデータ本体
    bool loaded = false; ///< ロード済みフラグ
  };
  /// @brief 現在再生中のVoice情報
  struct ActiveVoice {
    SourceVoicePtr v{};    ///< SourceVoice本体
    int slot = -1;         ///< SE識別子
    float volScale = 1.0f; ///< 再生時の個別音量倍率
  };

  ComPtr<IXAudio2> xaudio_; ///< XAudio2インターフェース
  MasterVoicePtr master_{};  ///< マスタリングボイス

  std::unordered_map<int, Entry> entries_; ///< IDごとのSEリソース管理
  std::vector<ActiveVoice> actives_;       ///< 現在再生中のVoiceリスト
  float volume_ = 1.0f;                    ///< マスター音量
};
