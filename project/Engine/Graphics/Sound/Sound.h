#pragma once
#include <cstdint>
#include <memory>
#include <windows.h>
#include <wrl/client.h>
#include <xaudio2.h>

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "xaudio2.lib")

struct ChunkHeader {
  char id[4];   // チャンクの識別子
  int32_t size; // チャンクのサイズ
};

struct RiffHeader {
  ChunkHeader chunk; // チャンクヘッダー
  char type[4];      // RIFFのタイプ（例: "WAVE"）
};

struct FormatChunk {
  ChunkHeader chunk; // チャンクヘッダー
  WAVEFORMATEX fmt;
};

struct SoundData {
  WAVEFORMATEX wfex{};               // PCM 前提
  std::unique_ptr<BYTE[]> pBuffer{}; // 音声データ（PCM）
  unsigned int bufferSize = 0;       // バイト数
};

// WAVローダ（互換維持）
SoundData SoundLoadWave(const char *filename);

// 拡張子で自動判定して読み込む（.wav / .mp3）
SoundData SoundLoadAudio(const char *filename);

// SoundData を空にする
void SoundUnload(SoundData *soundData);

IXAudio2SourceVoice *SoundPlayWave(IXAudio2 *xaudio2,
                                   const SoundData &soundData, float volume,
                                   bool loop);

void SoundStopWave(IXAudio2SourceVoice *pSourceVoice);

class Sound {
public:
  Sound();
  ~Sound();

  Sound(const Sound &) = delete;
  Sound &operator=(const Sound &) = delete;
  Sound(Sound &&) noexcept = default;
  Sound &operator=(Sound &&) noexcept = default;

  void Initialize(const char *filename); // 内部で SoundLoadAudio を使う

  void SoundImGui(const char *soundname);

  void SetVolume(float volume); // 音量設定
  float GetVolume() const;      // 音量取得

  void Play(bool loop = false); // 再生
  void Stop();                  // 停止

  void AllStop(); // 全サウンド停止

  void Unload(); // サウンドデータ解放

private:
  struct MasterVoiceDeleter {
    void operator()(IXAudio2MasteringVoice *v) const noexcept {
      if (v) {
        v->DestroyVoice();
      }
    }
  };
  struct SourceVoiceDeleter {
    void operator()(IXAudio2SourceVoice *v) const noexcept {
      if (v) {
        v->Stop(0);
        v->DestroyVoice();
      }
    }
  };

  ComPtr<IXAudio2> xAudio2;
  std::unique_ptr<IXAudio2MasteringVoice, MasterVoiceDeleter> masteringVoice{};
  std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter> voice{};
  SoundData soundData{};

  bool isLoop = false;
  float volume = 1.0f;
};
