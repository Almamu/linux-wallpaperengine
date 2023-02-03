#pragma once

#include <string>
#include <filesystem>
namespace Steam::FileSystem
{
    std::filesystem::path workshopDirectory (int appID, const std::string& contentID);
    std::filesystem::path appDirectory (const std::string& appDirectory, const std::string& path);
}