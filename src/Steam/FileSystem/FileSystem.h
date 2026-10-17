#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Steam::FileSystem {
std::filesystem::path workshopDirectory (int appID, const std::string& contentID);
std::vector<std::filesystem::path> workshopDirectories (int appID);
std::filesystem::path appDirectory (const std::string& appDirectory, const std::string& path);
} // namespace Steam::FileSystem