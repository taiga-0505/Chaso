#pragma once
#include <chrono>
#include <thread>

class FixFps {
public:

  void Initialize();

  // Present & Fence待ちの直後に毎フレーム呼ぶ
  void Update();

private:
	std::chrono::steady_clock::time_point reference_;
};
