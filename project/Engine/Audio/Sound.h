#pragma once
#include "EngineConfig.h"
#include <cstdint>
#include <memory>
#include <string>
#include <windows.h>
#include <wrl/client.h>
#include <xaudio2.h>

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "xaudio2.lib")

/// @brief チャンクヘッダー構造体
struct ChunkHeader {
  char id[4];   ///< チャンクの識別子
  int32_t size; ///< チャンクのサイズ
};

/// @brief RIFFヘッダー構造体
struct RiffHeader {
  ChunkHeader chunk; ///< チャンクヘッダー
  char type[4];      ///< RIFFのタイプ（例: "WAVE"）
};

/// @brief フォーマットチャンク構造体
struct FormatChunk {
  ChunkHeader chunk; ///< チャンクヘッダー
  WAVEFORMATEX fmt;  ///< 音声フォーマット情報
};

/// @brief 音声データ構造体
struct SoundData {
  WAVEFORMATEX wfex{};               ///< 音声フォーマット (PCM前提)
  std::unique_ptr<BYTE[]> pBuffer{}; ///< 音声データ本体
  unsigned int bufferSize = 0;       ///< データのバイト数
};

/// @brief WAVファイルをロードする
/// @param filename ファイルパス
/// @return ロードされた音声データ
SoundData SoundLoadWave(const char *filename);

/// @brief 拡張子を自動判別してオーディオファイルをロードする (.wav / .mp3)
/// @param filename ファイルパス
/// @return ロードされた音声データ
SoundData SoundLoadAudio(const char *filename);

/// @brief 音声データを解放する
/// @param soundData 解放対象の音声データのポインタ
void SoundUnload(SoundData *soundData);

/// @brief 音声を再生開始する
/// @param xaudio2 XAudio2インターフェース
/// @param soundData 再生する音声データ
/// @param volume 音量 (0.0 ~ 1.0)
/// @param loop ループ再生フラグ
/// @return 生成されたIXAudio2SourceVoiceのポインタ
IXAudio2SourceVoice *SoundPlayWave(IXAudio2 *xaudio2,
                                   const SoundData &soundData, float volume,
                                   bool loop);

/// @brief 再生中の音声を停止し、Voiceを破棄する
/// @param pSourceVoice 停止・破棄対象のSourceVoice
void SoundStopWave(IXAudio2SourceVoice *pSourceVoice);

/// @brief サウンド再生を管理するクラス
/// 単一のサウンドリソースのロード・再生・停止を制御する
class Sound {
public:
  /// @brief コンストラクタ
  Sound();
  /// @brief デストラクタ
  ~Sound();

  Sound(const Sound &) = delete;
  Sound &operator=(const Sound &) = delete;
  Sound(Sound &&) noexcept = default;
  Sound &operator=(Sound &&) noexcept = default;

  /// @brief 音声ファイルを読み込み、再生準備を行う
  /// @param filename ファイルパス
  void Initialize(const char *filename);

  /// @brief ImGuiを利用したサウンドデバッグUIを表示する
  /// @param soundname UIに表示する名前
  void SoundImGui(const char *soundname);

  /// @brief 音量を設定する
  /// @param volume 音量 (0.0 ~ 1.0)
  void SetVolume(float volume);

  /// @brief 現在の音量設定を取得する
  /// @return 音量 (0.0 ~ 1.0)
  float GetVolume() const;

  /// @brief 再生を開始する
  /// @param loop ループ再生するかどうか
  void Play(bool loop = false);

  /// @brief 再生を停止する
  void Stop();

  /// @brief 全ての再生を停止する
  void AllStop();

  /// @brief 音声リソースを解放する
  void Unload();

private:
  /// @brief MasterVoiceのカスタムデリータ
  struct MasterVoiceDeleter {
    void operator()(IXAudio2MasteringVoice *v) const noexcept {
      if (v) {
        v->DestroyVoice();
      }
    }
  };
  /// @brief SourceVoiceのカスタムデリータ
  struct SourceVoiceDeleter {
    void operator()(IXAudio2SourceVoice *v) const noexcept {
      if (v) {
        v->Stop(0);
        v->DestroyVoice();
      }
    }
  };

  ComPtr<IXAudio2> xAudio2; ///< XAudio2本体
  std::unique_ptr<IXAudio2MasteringVoice, MasterVoiceDeleter> masteringVoice{}; ///< マスタリングボイス
  std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter> voice{};             ///< 再生用ボイス
  SoundData soundData{};                                                        ///< 音声データ

  bool isLoop = false;   ///< ループ再生フラグ
  float volume = 1.0f;   ///< 音量
  std::string filePath_; ///< 読み込み中のファイルパス
};
