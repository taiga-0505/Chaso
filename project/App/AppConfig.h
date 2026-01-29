#pragma once
#include <array>
#include <string>
#ifndef RC_ENABLE_IMGUI
#if defined(_DEBUG) || defined(RC_DEVELOPMENT)
#define RC_ENABLE_IMGUI 1
#else
#define RC_ENABLE_IMGUI 0
#endif
#endif

/// <summary>
/// アプリケーションの起動設定を保持する
/// </summary>
struct AppConfig {
  int width = 1280;
  int height = 720;
  bool vsync = true;
  
#if defined(RC_DEVELOPMENT)
  std::string title = "CG3";
#elif defined(_DEBUG)
  std::string title = "ChasoEngine";
#else
  std::string title = "LE2B_03_オオシマ_タイガ_奈落ランナー";
#endif
  std::array<float, 4> clearColor{0.1f, 0.25f, 0.5f, 1.0f};
};
