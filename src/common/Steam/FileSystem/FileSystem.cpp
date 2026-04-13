#include <filesystem>
#include <fstream>

#include "FileSystem.h"

#include "WallpaperEngine/Logging/Log.h"
#include "vdf.hpp"

#include <ranges>

std::filesystem::path Steam::FileSystem::workshopDirectory (const std::string& base, int appID) {
	auto path = std::filesystem::path (base) / "steamapps" / "workshop" / "content" / std::to_string (appID);

	if (!std::filesystem::exists (path) || !std::filesystem::is_directory (path)) {
		sLog.exception ("Cannot find workshop directory for steam app ", appID, " on ", base);
	}

	return path;
}

std::filesystem::path Steam::FileSystem::appDirectory (const std::string& base, int appID) {
	auto libraryfolderspath = std::filesystem::path (base) / "steamapps" / "libraryfolders.vdf";
	auto appmanifestpath
		= std::filesystem::path (base) / "steamapps" / (std::string ("appmanifest_") + std::to_string (appID) + ".acf");

	if (!std::filesystem::exists (libraryfolderspath) || std::filesystem::is_directory (libraryfolderspath)) {
		sLog.exception ("Cannot find libraryfolders.vdf on ", base);
	}

	if (!std::filesystem::exists (appmanifestpath) || std::filesystem::is_directory (appmanifestpath)) {
		sLog.exception ("Cannot find appmanifest_", std::to_string (appID), ".acf on ", base);
	}

	std::stringstream buffer;

	{
		std::ifstream file (appmanifestpath);
		buffer << file.rdbuf ();
	}

	const auto appmanifestvdf = VdfParser::fromString (buffer.str ());
	const auto appstate = appmanifestvdf.getChild ("AppState");

	if (appstate.has_value () == false) {
		sLog.exception ("Cannot find AppState in appmanifest_", std::to_string (appID), ".acf on ", base);
	}

	const auto installdir = appstate->getChild ("installdir");

	if (installdir.has_value () == false) {
		sLog.exception ("Cannot find name in AppState in appmanifest_", std::to_string (appID), ".acf on ", base);
	}

	const auto installdirvalue = installdir->getValue ();

	if (installdirvalue.has_value () == false) {
		sLog.exception (
			"Cannot find value in name in AppState in appmanifest_", std::to_string (appID), ".acf on ", base
		);
	}

	buffer.str (std::string ());

	{
		std::ifstream file (libraryfolderspath);
		buffer << file.rdbuf ();
	}

	const auto vdf = VdfParser::fromString (buffer.str ());
	const auto libraryfolders = vdf.getChild ("libraryfolders");

	if (libraryfolders.has_value () == false) {
		sLog.exception ("Cannot find libraryfolders in libraryfolders.vdf on ", base);
	}

	const auto children = libraryfolders->getChildren ();

	if (children.has_value () == false) {
		sLog.exception ("Cannot find children in libraryfolders in libraryfolders.vdf on ", base);
	}

	for (const auto library : *children | std::views::values) {
		const auto path = library.getChild ("path");

		if (path.has_value () == false) {
			// weird library, doesn't have a path, huh?...
			continue;
		}

		const auto librarypath = path->getValue ();

		if (librarypath.has_value () == false) {
			continue;
		}

		// check if the appid is registered
		const auto apps = library.getChild ("apps");

		if (apps.has_value () == false) {
			// the library has no apps installed
			continue;
		}

		const auto appids = apps->getChildren ();

		if (appids.has_value () == false) {
			continue;
		}

		const auto appid = appids->find (std::to_string (appID));

		if (appid == appids->end ()) {
			continue;
		}

		// app is installed here, use it as path
		const auto finalpath = std::filesystem::path (*librarypath) / "steamapps" / "common" / *installdirvalue;

		if (std::filesystem::exists (finalpath) && std::filesystem::is_directory (finalpath)) {
			return finalpath;
		}
	}

	sLog.exception ("Cannot find app ", appID, " in libraryfolders.vdf on ", base);
}