#include "FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"
#include <filesystem>

std::filesystem::path Steam::FileSystem::workshopDirectory (const std::string& base, int appID) {
	auto path = std::filesystem::path (base) / "steamapps" / "workshop" / "content" / std::to_string (appID);

	if (!std::filesystem::exists (path) || !std::filesystem::is_directory (path)) {
		sLog.exception ("Cannot find workshop directory for steam app ", appID, " on ", base);
	}

	return path;
}

std::filesystem::path Steam::FileSystem::appDirectory (const std::string& base, const std::string& name) {
	auto path = std::filesystem::path (base) / "steamapps" / "common" / name;

	if (!std::filesystem::exists (path) || !std::filesystem::is_directory (path)) {
		sLog.exception ("Cannot find app directory for steam app ", name, " on ", base);
	}

	return path;
}