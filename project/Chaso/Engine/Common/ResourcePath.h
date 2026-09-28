#pragma once
#include <filesystem>
#include <string>

/// @file ResourcePath.h
/// @brief リソースパスの解決（ゲーム側 → エンジン側の順に探す）
/// @details エンジン本体は `project/Chaso/` 配下（ChasoEngine リポジトリの subtree）にあり、
///          エンジンが必要とするリソース（シェーダー・アイコン・フォントなど）は
///          `Chaso/Resources/` に置かれている。一方、コード中のパスは従来どおり
///          `"Resources/..."`（作業ディレクトリ = project/ 基準）で書かれているため、
///          ゲーム側の `Resources/` に無ければ `Chaso/Resources/` を探す。
///
///          ゲーム側で同名ファイルを置けばエンジン側を上書きできる（ゲーム側が優先）。
namespace Chaso {

/// @brief エンジンのルートディレクトリ（作業ディレクトリ基準）。既定は "Chaso"。
inline std::string &EngineRootRef_() {
  static std::string root = "Chaso";
  return root;
}

/// @brief エンジンのルートディレクトリを変更する（通常は変更不要）
inline void SetEngineRoot(const std::string &root) { EngineRootRef_() = root; }

/// @brief エンジンのルートディレクトリを取得する
inline const std::string &EngineRoot() { return EngineRootRef_(); }

/// @brief パスを解決する
/// @param path "Resources/..." 形式の相対パス（絶対パスならそのまま返す）
/// @return 実在するパス。ゲーム側 → エンジン側の順で探し、どちらにも無ければ入力をそのまま返す
inline std::filesystem::path ResolvePath(const std::filesystem::path &path) {
  namespace fs = std::filesystem;
  if (path.empty() || path.is_absolute()) {
    return path;
  }
  std::error_code ec;
  if (fs::exists(path, ec)) {
    return path;
  }
  const fs::path engine = fs::path(EngineRoot()) / path;
  if (fs::exists(engine, ec)) {
    return engine;
  }
  return path;
}

/// @brief std::string 版
inline std::string ResolvePath(const std::string &path) {
  return ResolvePath(std::filesystem::path(path)).generic_string();
}

/// @brief const char* 版
inline std::string ResolvePath(const char *path) {
  return ResolvePath(std::string(path ? path : ""));
}

/// @brief std::wstring 版（DXC など wchar_t API 向け）
inline std::wstring ResolvePath(const std::wstring &path) {
  return ResolvePath(std::filesystem::path(path)).generic_wstring();
}

} // namespace Chaso
