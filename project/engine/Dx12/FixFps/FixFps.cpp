#include "FixFps.h"

void FixFps::Initialize() { reference_ = std::chrono::steady_clock::now(); }

void FixFps::Update() {

  // 1 / 60 秒ピッタリの時間
  const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
  // 1/60よりわずかに早い時間
  const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

  // 現在の時間を取得
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  // 前回の記録からの経過時間を取得
  std::chrono::microseconds elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

  // 経過時間が1/60秒未満なら待機
  if (elapsed < kMinTime) {
    // 1/60秒になるまで微小なスリープを繰り返す
      while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }
  }

  // 現在の時間を記録する
  reference_ = std::chrono::steady_clock::now();
}
