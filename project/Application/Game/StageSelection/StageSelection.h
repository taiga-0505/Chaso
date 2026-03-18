#pragma once
#include <string>

class StageSelection {
public:
  static void SetCsvPath(const std::string &path);
  static const std::string &GetCsvPath();
  static const std::string &GetCsvPathOrDefault();
  static void Clear();
};
