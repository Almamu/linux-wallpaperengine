#include "FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>

std::vector<std::string> appDirectoryPaths = {
    ".steam/steam/steamapps/common",
    ".local/share/Steam/steamapps/common",
    ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common",
    "snap/steam/common/.local/share/Steam/steamapps/common",
};

std::vector<std::string> workshopDirectoryPaths = {
    ".local/share/Steam/steamapps/workshop/content",
    ".steam/steam/steamapps/workshop/content",
    ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/workshop/content",
    "snap/steam/common/.local/share/Steam/steamapps/workshop/content",
};

std::filesystem::path detectHomepath () {
    char* home = getenv ("HOME");

    if (home == nullptr)
        sLog.exception ("Cannot find home directory for the current user");

    const std::filesystem::path path = home;

    if (!std::filesystem::is_directory (path))
        sLog.exception ("Cannot find home directory for current user, ", home, " is not a directory");

    return path;
}

std::filesystem::path Steam::FileSystem::workshopDirectory (int appID, const std::string& contentID) {
    auto homepath = detectHomepath ();

    for (const auto& current : workshopDirectoryPaths) {
        auto currentpath = std::filesystem::path (homepath) / current / std::to_string (appID) / contentID;

        if (!std::filesystem::exists (currentpath) || !std::filesystem::is_directory (currentpath))
            continue;

        return currentpath;
    }

    sLog.exception ("Cannot find workshop directory for steam app ", appID, " and content ", contentID);
}

std::filesystem::path Steam::FileSystem::appDirectory (const std::string& appDirectory, const std::string& path) {
    auto homepath = detectHomepath ();

    for (const auto& current : appDirectoryPaths) {
        auto currentpath = std::filesystem::path (homepath) / current / appDirectory / path;

        if (!std::filesystem::exists (currentpath) || !std::filesystem::is_directory (currentpath))
            continue;

        return currentpath;
    }

    sLog.exception ("Cannot find directory for steam app ", appDirectory, ": ", path);
}