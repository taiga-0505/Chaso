#include "StageSelection.h"

namespace {
std::string gCsvPath;
const std::string kDefaultCsvPath = "Resources/Stage/Stage1/stage1.csv";
} // namespace

void StageSelection::SetCsvPath(const std::string &path) { gCsvPath = path; }

const std::string &StageSelection::GetCsvPath() { return gCsvPath; }

const std::string &StageSelection::GetCsvPathOrDefault() {
  return gCsvPath.empty() ? kDefaultCsvPath : gCsvPath;
}

void StageSelection::Clear() { gCsvPath.clear(); }
