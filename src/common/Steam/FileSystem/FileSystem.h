#pragma once

#include <filesystem>
#include <string>

namespace Steam::FileSystem {
std::filesystem::path workshopDirectory (const std::string& base, int appID);
std::filesystem::path appDirectory (const std::string& base, int appID);
} // namespace Steam::FileSystem