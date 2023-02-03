#include "FileSystem.h"
#include "WallpaperEngine/Logging/CLog.h"
#include <climits>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <sys/stat.h>

const char* assets_default_paths [] = {
    ".steam/steam/steamapps/common",
    ".local/share/Steam/steamapps/common",
    nullptr
};

const char* workshop_content_default_paths [] = {
    ".local/share/Steam/steamapps/workshop/content",
    ".steam/steam/steamapps/workshop/content",
    nullptr
};

std::filesystem::path detectHomepath ()
{
    char* home = getenv ("HOME");

    if (home == nullptr)
        sLog.exception ("Cannot find home directory for the current user");

    std::filesystem::path path = home;

    if (std::filesystem::is_directory (path) == false)
        sLog.exception ("Cannot find home directory for current user, ", home, " is not a directory");

    return home;
}

std::filesystem::path Steam::FileSystem::workshopDirectory (int appID, const std::string& contentID)
{
    auto homepath = detectHomepath ();

    for (const char** current = workshop_content_default_paths; *current != nullptr; current ++)
    {
        auto currentpath = std::filesystem::path (homepath) / *current / std::to_string (appID) / contentID;

        if (std::filesystem::exists (currentpath) == false || std::filesystem::is_directory (currentpath) == false)
            continue;

        return currentpath;
    }

    sLog.exception ("Cannot find workshop directory for steam app ", appID, " and content ", contentID);
}

std::filesystem::path Steam::FileSystem::appDirectory (const std::string& appDirectory, const std::string& path)
{
    auto homepath = detectHomepath ();

    for (const char** current = assets_default_paths; *current != nullptr; current ++)
    {
        auto currentpath = std::filesystem::path (homepath) / *current / appDirectory / path;

        if (std::filesystem::exists (currentpath) == false || std::filesystem::is_directory (currentpath) == false)
            continue;

        return currentpath;
    }

    sLog.exception ("Cannot find directory for steam app ", appDirectory, ": ", path);
}