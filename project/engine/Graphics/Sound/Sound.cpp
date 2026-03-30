#include "Sound.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <vector>

// ==== Media Foundation ====
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// =========================
// ユーティリティ
// =========================
static bool HasExtInsensitive(const char *path, const char *ext) {
  if (!path || !ext)
    return false;
  std::string s(path);
  std::string e(ext);
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  std::transform(e.begin(), e.end(), e.begin(), ::tolower);
  if (e.size() && e[0] != '.')
    e = "." + e;
  if (s.size() < e.size())
    return false;
  return s.rfind(e) == s.size() - e.size();
}

static std::wstring ToWideFromUTF8(const char *utf8) {
  if (!utf8)
    return L"";
  int lenW = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (lenW <= 0)
    return L"";
  std::wstring w(lenW, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], lenW);
  if (!w.empty() && w.back() == L'\0')
    w.pop_back();
  return w;
}

// =========================
// WAV ローダ（既存）
// =========================
SoundData SoundLoadWave(const char *filename) {

  // ==========================
  // ファイルオープン
  // ==========================
  std::ifstream file;
  file.open(filename, std::ios_base::binary);
  assert(file.is_open());

  // ==========================
  // .wavデータの読み込み
  // ==========================
  RiffHeader riff;
  file.read((char *)&riff, sizeof(riff));

  if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
    assert(0);
  }
  if (strncmp(riff.type, "WAVE", 4) != 0) {
    assert(0);
  }

  FormatChunk format = {};
  file.read((char *)&format, sizeof(ChunkHeader));
  if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
    assert(0);
  }

  assert(format.chunk.size <= sizeof(format.fmt));
  file.read((char *)&format.fmt, format.chunk.size);

  ChunkHeader data;
  file.read((char *)&data, sizeof(data));

  if (strncmp(data.id, "JUNK", 4) == 0) {
    // JUNK チャンクが存在する場合は読み飛ばす
    file.seekg(data.size, std::ios_base::cur);
    file.read((char *)&data, sizeof(data));
  }

  if (strncmp(data.id, "data", 4) != 0) {
    assert(0);
  }

  auto pBuffer = std::make_unique<BYTE[]>(static_cast<size_t>(data.size));
  file.read(reinterpret_cast<char *>(pBuffer.get()), data.size);

  file.close();

  // ==========================
  // return
  // ==========================
  SoundData soundData{};
  soundData.wfex = format.fmt; // PCM 前提
  soundData.pBuffer = std::move(pBuffer);
  soundData.bufferSize = static_cast<unsigned int>(data.size);

  return soundData;
}

// =========================
// Media Foundation 初期化
// =========================
static void EnsureMediaFoundationStartup() {
  static bool initialized = false;
  if (!initialized) {
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    assert(SUCCEEDED(hr));
    initialized = true;
  }
}

// =========================
// MF を使った汎用オーディオ読み込み（MP3→PCM）
// =========================
static SoundData SoundLoadWithMediaFoundation(const wchar_t *wpath) {
  EnsureMediaFoundationStartup();

  Microsoft::WRL::ComPtr<IMFSourceReader> reader;
  HRESULT hr = MFCreateSourceReaderFromURL(wpath, nullptr, &reader);
  assert(SUCCEEDED(hr));

  // 出力を PCM に固定
  Microsoft::WRL::ComPtr<IMFMediaType> outType;
  hr = MFCreateMediaType(&outType);
  assert(SUCCEEDED(hr));
  hr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  assert(SUCCEEDED(hr));
  hr = outType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
  assert(SUCCEEDED(hr));
  hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr,
                                   outType.Get());
  assert(SUCCEEDED(hr));

  // 実際の PCM タイプを取得して WAVEFORMATEX に変換
  Microsoft::WRL::ComPtr<IMFMediaType> currentType;
  hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                   &currentType);
  assert(SUCCEEDED(hr));

  WAVEFORMATEX *pwfx = nullptr;
  UINT32 cbwfx = 0;
  hr = MFCreateWaveFormatExFromMFMediaType(currentType.Get(), &pwfx, &cbwfx);
  assert(SUCCEEDED(hr));
  // pwfx は CoTaskMemFree で解放する

  std::vector<BYTE> pcmBytes;
  for (;;) {
    DWORD flags = 0;
    Microsoft::WRL::ComPtr<IMFSample> sample;
    hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr,
                            &flags, nullptr, &sample);
    assert(SUCCEEDED(hr));

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
      break;
    }
    if (!sample) {
      continue;
    }

    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    hr = sample->ConvertToContiguousBuffer(&buffer);
    assert(SUCCEEDED(hr));

    BYTE *pData = nullptr;
    DWORD cbData = 0;
    hr = buffer->Lock(&pData, nullptr, &cbData);
    assert(SUCCEEDED(hr));

    size_t oldSize = pcmBytes.size();
    pcmBytes.resize(oldSize + cbData);
    memcpy(pcmBytes.data() + oldSize, pData, cbData);

    hr = buffer->Unlock();
    assert(SUCCEEDED(hr));
  }

  SoundData sd{};
  sd.wfex = *pwfx; // 構造体コピー（PCM）
  sd.bufferSize = static_cast<unsigned int>(pcmBytes.size());
  sd.pBuffer = std::make_unique<BYTE[]>(sd.bufferSize);
  memcpy(sd.pBuffer.get(), pcmBytes.data(), sd.bufferSize);

  CoTaskMemFree(pwfx);
  return sd;
}

// =========================
// 拡張子で自動判定して読み込み
// =========================
SoundData SoundLoadAudio(const char *filename) {
  if (HasExtInsensitive(filename, ".wav")) {
    return SoundLoadWave(filename);
  }
  if (HasExtInsensitive(filename, ".mp3")) {
    std::wstring w = ToWideFromUTF8(filename);
    return SoundLoadWithMediaFoundation(w.c_str());
  }
  // 他形式は必要になったらここで分岐追加（.wma, .aac など）
  assert(!"Unsupported audio format");
  return {};
}

void SoundUnload(SoundData *soundData) {
  if (!soundData) {
    return;
  }
  soundData->pBuffer.reset();
  soundData->bufferSize = 0;
  soundData->wfex = {};
}

IXAudio2SourceVoice *SoundPlayWave(IXAudio2 *xaudio2,
                                   const SoundData &soundData, float volume,
                                   bool loop) {
  HRESULT result;

  IXAudio2SourceVoice *pSourceVoice = nullptr;
  result = xaudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
  assert(SUCCEEDED(result));

  pSourceVoice->SetVolume(volume);

  XAUDIO2_BUFFER buf{};
  buf.pAudioData = soundData.pBuffer.get();
  buf.AudioBytes = soundData.bufferSize;
  buf.Flags = XAUDIO2_END_OF_STREAM;
  buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

  result = pSourceVoice->SubmitSourceBuffer(&buf);
  assert(SUCCEEDED(result));
  result = pSourceVoice->Start();
  assert(SUCCEEDED(result));

  return pSourceVoice;
}

void SoundStopWave(IXAudio2SourceVoice *pSourceVoice) {
  if (pSourceVoice) {
    pSourceVoice->Stop();
    pSourceVoice->DestroyVoice();
  }
}

Sound::Sound() {
  // XAudio2の初期化
  HRESULT hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
  assert(SUCCEEDED(hr));
}

Sound::~Sound() {
  // unique_ptr のデリータで止めて破棄される
  Stop();
  Unload();
  masteringVoice.reset();
}

void Sound::Initialize(const char *filename) {

  Stop();
  SoundUnload(&soundData);

  if (!masteringVoice) {
    IXAudio2MasteringVoice *raw = nullptr;
    HRESULT hr = xAudio2->CreateMasteringVoice(&raw);
    assert(SUCCEEDED(hr));
    masteringVoice.reset(raw);
  }
  // ここを WAV 固定から、拡張子自動判定ローダに変更
  soundData = SoundLoadAudio(filename);
}

void Sound::SoundImGui(const char *soundname) {
#if RC_ENABLE_IMGUI

  std::string Label = std::string(soundname);

  if (ImGui::CollapsingHeader(Label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox((std::string("ループ再生##") + Label).c_str(), &isLoop);

    // 音量スライダー
    ImGui::SliderFloat((std::string("音量##") + Label).c_str(), &volume, 0.0f,
                       1.0f, "%.2f");
    if (voice) {
      voice->SetVolume(volume); // 再生中はリアルタイムで反映
    }

    if (ImGui::Button((std::string("再生##") + Label).c_str())) {
      voice.reset(SoundPlayWave(xAudio2.Get(), soundData, volume, isLoop));
    }

    ImGui::SameLine();

    if (ImGui::Button((std::string("停止##") + Label).c_str())) {
      Stop();
    }

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
  }

#endif
}

void Sound::SetVolume(float volume_) {
  volume = volume_;
  if (voice) {
    voice->SetVolume(volume);
  }
}

float Sound::GetVolume() const { return volume; }

void Sound::Play(bool loop) {
  isLoop = loop;
  voice.reset(SoundPlayWave(xAudio2.Get(), soundData, volume, isLoop));
}

void Sound::Stop() { voice.reset(); }

void Sound::AllStop() { Stop(); }

void Sound::Unload() {
  Stop();
  SoundUnload(&soundData);
}
