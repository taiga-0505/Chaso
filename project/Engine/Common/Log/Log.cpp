#include "Log.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <Windows.h>
#include <format>
#include <string>

void Log::Initialize() {

  // ログのディレクトリを用意
  std::filesystem::create_directory("../logs");
  // 現在の時間を取得(UTC)
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  // ログファイルの名前にコンマ何秒はいらないので、削って秒にする
  std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
      nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  // 日本時間に変換
  std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};
  // formatを使って年月日_時分秒の文字列に変換
  std::string dateString = std::format("{:%Y-%m-%d_%H-%M-%S}", localTime);
  // 時間を使ってファイル名を決定
  std::string logFilePath = std::string("../logs/") + dateString + ".log";
  // ファイルを作って書き込み準備
  std::ofstream logStream(logFilePath);
}

void Log::WriteLog(std::ostream &os, const std::string &message) {
  os << message << std::endl;
  OutputDebugStringA(message.c_str());
}

void Log::WriteLog(const std::string &message) {
  OutputDebugStringA(message.c_str());
}

std::wstring Log::ConvertString(const std::string &str) {
  // stringのサイズを取得
  int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
  // wstringのサイズを取得
  std::wstring wstr(size, L'\0');
  // stringをwstringに変換
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
  return wstr;
}

// wstringをstringに変換する関数
std::string Log::ConvertString(const std::wstring &wstr) {
  // wstringのサイズを取得
  int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
                                 nullptr, nullptr);
  // stringのサイズを取得
  std::string str(size, '\0');
  // wstringをstringに変換
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr,
                      nullptr);
  return str;
}
