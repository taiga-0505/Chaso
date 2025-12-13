#pragma once
#include <array>
#include <string>
#ifndef RC_ENABLE_IMGUI
#if defined(_DEBUG)
#define RC_ENABLE_IMGUI 1
#else
#define RC_ENABLE_IMGUI 0
#endif
#endif

struct AppConfig {
  int width = 1280;
  int height = 720;
  bool vsync = true;
  std::string title = "LE2B_03_オオシマ_タイガ";
  std::array<float, 4> clearColor{0.1f, 0.25f, 0.5f, 1.0f};
};
