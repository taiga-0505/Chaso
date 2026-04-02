#pragma once
#include <ostream>

class Log {

public:
  void Initialize();

  void WriteLog(std::ostream &os, const std::string &message);

  void WriteLog(const std::string &message);

  // ログ出力（静的メソッド）
  static void Print(const std::string &message);

  // stringをwstringに変換する関数
  std::wstring ConvertString(const std::string &str);

  // wstringをstringに変換する関数
  std::string ConvertString(const std::wstring &wstr);
};
