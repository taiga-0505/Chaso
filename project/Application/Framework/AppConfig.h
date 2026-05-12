#pragma once
#include <array>
#include <string>
#include "EngineConfig.h"

/// @struct AppConfig
/// @brief アプリケーションの起動設定を保持する構造体
/// @details 画面解像度、ウィンドウタイトル、V-Sync の有効無効、および画面クリアカラーを定義します。
struct AppConfig {
  int width = 1280;  ///< ウィンドウの幅
  int height = 720; ///< ウィンドウの高さ
  bool vsync = true; ///< 垂直同期 (V-Sync) を有効にするか

#if defined(RC_DEVELOPMENT)
  std::string title = "CG3"; ///< 開発ビルド時のタイトル
#elif defined(_DEBUG)
  std::string title = "ChasoEngine"; ///< デバッグビルド時のタイトル
#else
  std::string title = "LE2B_03_オオシマ_タイガ_奈落ランナー"; ///< リリースビルド時のタイトル
#endif

  std::array<float, 4> clearColor{0.1f, 0.25f, 0.5f, 1.0f}; ///< 画面のクリアカラー (RGBA)
};
