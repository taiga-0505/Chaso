#pragma once
#include <ostream>
#include <fstream>
#include <string>

class Log {

public:
  static void Initialize();

  static void Finalize();

  void WriteLog(std::ostream &os, const std::string &message);

  void WriteLog(const std::string &message);

  // ログ出力（静的メソッド）
  static void Print(const std::string &message);

  // stringをwstringに変換する関数
  std::wstring ConvertString(const std::string &str);

  // wstringをstringに変換する関数
  std::string ConvertString(const std::wstring &wstr);

  /// <summary>
  /// パスの区切り文字を '/' に統一する
  /// </summary>
  static std::string NormalizePath(const std::string &path);

private:
  static std::ofstream sLogFile_;
};
