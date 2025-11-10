#pragma once
#include <cstdint>
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
  WAVEFORMATEX wfex;
  BYTE *pBuffer;           // 音声データのバッファ（PCM）
  unsigned int bufferSize; // バッファのサイズ（バイト）
};

// 既存のWAVローダ（互換維持）
SoundData SoundLoadWave(const char *filename);

// 新規：拡張子で自動判定して読み込む（.wav / .mp3）
SoundData SoundLoadAudio(const char *filename);

void SoundUnload(SoundData *soundData);

IXAudio2SourceVoice *SoundPlayWave(IXAudio2 *xaudio2,
                                   const SoundData &soundData, float volume,
                                   bool loop);

void SoundStopWave(IXAudio2SourceVoice *pSourceVoice);

class Sound {

public:
  Sound();
  ~Sound();

  void Initialize(const char *filename); // 内部で SoundLoadAudio を使う

  void SoundImGui(const char *soundname);

  void SetVolume(float volume); // 音量設定
  float GetVolume() const;      // 音量取得

  void Play(bool loop = false); // 再生
  void Stop();                  // 停止

  void AllStop(); // 全サウンド停止

  void Unload(); // サウンドデータ解放

private:
  ComPtr<IXAudio2> xAudio2;
  IXAudio2MasteringVoice *masteringVoice = nullptr;
  IXAudio2SourceVoice *voice = nullptr;
  SoundData soundData;

  bool isLoop = false;
  float volume = 1.0f;
};
