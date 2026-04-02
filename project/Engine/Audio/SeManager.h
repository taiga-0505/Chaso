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

// 必要に応じて増減してOK（使い方はBgmGroupと同じ感覚）
enum class SeId {
  Cursor = 0,
  Decide = 1,
  Cancel = 2,
  Hit = 3,
  Damage = 4,
  Clear = 5,
  Count
};

class SeManager {
public:
  void Init(float defaultVolume = 1.0f) {
    HRESULT hr = XAudio2Create(&xaudio_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));

    IXAudio2MasteringVoice *rawMaster = nullptr;
    hr = xaudio_->CreateMasteringVoice(&rawMaster);
    assert(SUCCEEDED(hr));
    master_.reset(rawMaster);

    volume_ = defaultVolume;
  }

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

  // パス登録（あとから差し替えOK）
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

  // その場で鳴らす（volScaleは個別音量の倍率）
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

  // 鳴り終わったVoiceを掃除（毎フレ呼ぶ）
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

  // マスター音量（0.0〜1.0想定）
  void SetMasterVolume(float v) {
    volume_ = (std::max)(0.0f, v);
    // 再生中にも反映
    for (auto &av : actives_) {
      if (av.v)
        av.v->SetVolume(volume_ * av.volScale);
    }
  }
  float MasterVolume() const { return volume_; }

  // 全停止（シーン切替時などに）
  void StopAll() { actives_.clear(); }

private:
  struct SourceVoiceDeleter {
    void operator()(IXAudio2SourceVoice *v) const noexcept {
      if (v) {
        v->Stop(0);
        v->DestroyVoice();
      }
    }
  };
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

  struct Entry {
    std::string path;
    SoundData data{}; // PCM
    bool loaded = false;
  };
  struct ActiveVoice {
    SourceVoicePtr v{};
    int slot = -1;
    float volScale = 1.0f;
  };

  ComPtr<IXAudio2> xaudio_;
  MasterVoicePtr master_{};

  std::unordered_map<int, Entry> entries_;
  std::vector<ActiveVoice> actives_;
  float volume_ = 1.0f;
};
