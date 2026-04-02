#include "FixFps.h"

void FixFps::Initialize() {
    reference_ = std::chrono::steady_clock::now(); 
}

void FixFps::Update() {

  // 1 / 60 秒ピッタリの時間
  const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

  // 現在の時間を取得
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  // 前回の記録からの経過時間を取得
  std::chrono::microseconds elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

  // 経過時間が1/60秒未満なら待機
  if (elapsed < kMinTime) {
    // 1/60秒になるまでスリープとビジーウェイトで待機
    while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
      // 残り時間が1ms以上あればスリープし、それ未満ならビジーウェイトに切り替えて精度を出す
      if (std::chrono::steady_clock::now() - reference_ < kMinTime - std::chrono::microseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
      }
    }
  }

  // 次のフレームの基準時刻を理想的な時間グリッドに従って更新
  reference_ += kMinTime;

  // 処理が重すぎて1/60秒以上経過してしまった場合、基準時刻を現在時刻に補正
  if (std::chrono::steady_clock::now() - reference_ > kMinTime) {
    reference_ = std::chrono::steady_clock::now();
  }
}
