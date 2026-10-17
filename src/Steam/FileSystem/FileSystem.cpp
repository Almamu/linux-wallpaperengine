#include "FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace {

std::filesystem::path detectHomepath () {
    char* home = getenv ("HOME");

    if (home == nullptr) {
	sLog.exception ("Cannot find home directory for the current user");
    }

    const std::filesystem::path path = home;

    if (!std::filesystem::is_directory (path)) {
	sLog.exception ("Cannot find home directory for current user, ", home, " is not a directory");
    }

    return path;
}

void appendUnique (
    std::vector<std::filesystem::path>& result, std::set<std::string>& seen, const std::filesystem::path& path
) {
    const auto normalized = path.lexically_normal ();
    const auto key = normalized.string ();
    if (seen.insert (key).second) {
	result.push_back (normalized);
    }
}

std::string unescapeVdfPath (std::string value) {
    std::string result;
    result.reserve (value.size ());
    for (std::size_t i = 0; i < value.size (); ++i) {
	if (value[i] == '\\' && i + 1 < value.size () && (value[i + 1] == '\\' || value[i + 1] == '"')) {
	    result.push_back (value[++i]);
	} else {
	    result.push_back (value[i]);
	}
    }
    return result;
}

std::vector<std::filesystem::path> steamappsDirectories () {
    const auto home = detectHomepath ();
    std::vector<std::filesystem::path> primary;
    if (const char* xdgDataHome = getenv ("XDG_DATA_HOME"); xdgDataHome != nullptr && *xdgDataHome != '\0') {
	primary.emplace_back (std::filesystem::path (xdgDataHome) / "Steam/steamapps");
    }
    primary.insert (
	primary.end (),
	{
	    home / ".local/share/Steam/steamapps",
	    home / ".steam/steam/steamapps",
	    home / ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps",
	    home / "snap/steam/common/.local/share/Steam/steamapps",
	}
    );

    std::vector<std::filesystem::path> result;
    std::set<std::string> seen;
    const std::regex pathEntry ("\\\"path\\\"\\s*\\\"([^\\\"]+)\\\"", std::regex::icase);
    const std::regex legacyEntry ("^\\s*\\\"\\d+\\\"\\s*\\\"([^\\\"]+)\\\"\\s*$");

    for (const auto& steamapps : primary) {
	appendUnique (result, seen, steamapps);

	std::ifstream libraries (steamapps / "libraryfolders.vdf");
	if (!libraries.is_open ()) {
	    continue;
	}

	std::string line;
	while (std::getline (libraries, line)) {
	    std::smatch match;
	    if (!std::regex_search (line, match, pathEntry) && !std::regex_match (line, match, legacyEntry)) {
		continue;
	    }

	    const std::filesystem::path library = unescapeVdfPath (match[1].str ());
	    if (!library.is_absolute ()) {
		continue;
	    }
	    appendUnique (result, seen, library / "steamapps");
	}
    }

    return result;
}

} // namespace

std::vector<std::filesystem::path> Steam::FileSystem::workshopDirectories (int appID) {
    std::vector<std::filesystem::path> result;

    for (const auto& steamapps : steamappsDirectories ()) {
	const auto currentpath = steamapps / "workshop/content" / std::to_string (appID);

	if (!std::filesystem::exists (currentpath) || !std::filesystem::is_directory (currentpath)) {
	    continue;
	}

	result.push_back (currentpath);
    }

    return result;
}

std::filesystem::path Steam::FileSystem::workshopDirectory (int appID, const std::string& contentID) {
    for (const auto& root : workshopDirectories (appID)) {
	const auto currentpath = root / contentID;

	if (std::filesystem::exists (currentpath) && std::filesystem::is_directory (currentpath)) {
	    return currentpath;
	}
    }

    sLog.exception ("Cannot find workshop directory for steam app ", appID, " and content ", contentID);
}

std::filesystem::path Steam::FileSystem::appDirectory (const std::string& appDirectory, const std::string& path) {
    for (const auto& steamapps : steamappsDirectories ()) {
	const auto currentpath = steamapps / "common" / appDirectory / path;

	if (!std::filesystem::exists (currentpath) || !std::filesystem::is_directory (currentpath)) {
	    continue;
	}

	return currentpath;
    }

    sLog.exception ("Cannot find directory for steam app ", appDirectory, ": ", path);
}
