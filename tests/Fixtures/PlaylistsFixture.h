#pragma once

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Config.h"
#include "WallpaperEngine/Utils/UUID.h"

#include <filesystem>
#include <fstream>

using namespace WallpaperEngine::Data::Model;

inline std::map<PlaylistTransition, std::string> transitionToValue = {
	{PlaylistTransition_None, "none"},
	{PlaylistTransition_Random, "random"},
	{PlaylistTransition_NoneReducedFlicker, "-2"},
	{PlaylistTransition_Fade, "0"},
	{PlaylistTransition_Mosaic, "1"},
	{PlaylistTransition_Diffuse, "2"},
	{PlaylistTransition_HorizontalSlide, "3"},
	{PlaylistTransition_VerticalSlide, "4"},
	{PlaylistTransition_HorizontalFade, "5"},
	{PlaylistTransition_VerticalFade, "6"},
	{PlaylistTransition_Clouds, "7"},
	{PlaylistTransition_BurnPaper, "8"},
	{PlaylistTransition_Circular, "9"},
	{PlaylistTransition_Zipper, "10"},
	{PlaylistTransition_Door, "11"},
	{PlaylistTransition_Lines, "12"},
	{PlaylistTransition_Zoom, "13"},
	{PlaylistTransition_Drip, "14"},
	{PlaylistTransition_Pixelate, "15"},
	{PlaylistTransition_Bricks, "16"},
	{PlaylistTransition_Paint, "17"},
	{PlaylistTransition_FadeToBlack, "18"},
	{PlaylistTransition_Twister, "19"},
	{PlaylistTransition_BlackHole, "20"},
	{PlaylistTransition_CRT, "21"},
	{PlaylistTransition_RadialWipe, "22"},
	{PlaylistTransition_GlassShatter, "23"},
	{PlaylistTransition_Bullets, "24"},
	{PlaylistTransition_Ice, "25"},
	{PlaylistTransition_Boilover, "26"},
};

inline std::map<PlaylistMode, std::string> modeToValue = {
	{PlaylistMode_Timer, "timer"},
	{PlaylistMode_Logon, "logon"},
	{PlaylistMode_Daytime, "daytime"},
	{PlaylistMode_Weekday, "dayofweek"},
	{PlaylsitMode_Never, "never"},
};

struct PlaylistsFixture {
	PlaylistsFixture() {
		// uuidv4 should be more than enough
		root =
			std::filesystem::temp_directory_path () / WallpaperEngine::Utils::UUID::UUIDv4 () /
			"common" / "wallpaper_engine";
		bgroot = root / ".." / "..";

		std::filesystem::create_directories (root);
		std::filesystem::create_directories (root / "assets");
	}

	~PlaylistsFixture() {
		std::filesystem::remove_all (root);
	}

	void addPlaylist (
		const std::string& name,
		const PlaylistMode mode,
		const PlaylistOrder order,
		const PlaylistTransition transition,
		int delay,
		int transitiontime,
		const std::vector<std::filesystem::path>& backgrounds) {
		std::vector<std::filesystem::path> backgroundsFinal = {};

		std::ranges::transform (
			backgrounds,
			std::back_inserter (backgroundsFinal),
			[this] (const std::filesystem::path& path) { return bgroot / path; }
		);

		std::ranges::for_each (backgroundsFinal, [](const std::filesystem::path& path) {
			std::filesystem::create_directories (path.parent_path());
		});

		const WallpaperEngine::Data::JSON::JSON playlist = {
			{"items", backgroundsFinal},
			{"name", name},
			{
				"settings", {
					{"delay", delay},
					{"transitiontime", transitiontime},
					{"transition", transitionToValue [transition]},
					{"mode", modeToValue [mode]},
					{"order", order == PlaylistOrder_Random ? "random" : "sorted"}
				}
			}
		};

		contents["steamuser"]["general"]["playlists"].push_back (playlist);
	}

	void createConfigFile () {
		std::ofstream (root / "config.json") << contents;
	}

	WallpaperEngine::Data::JSON::JSON contents = {
		{"?installdirectory", root},
		{
			"steamuser", {
				{"general", {
						{"playlists", WallpaperEngine::Data::JSON::JSON::array ()}
					}
				}
			}
		}
	};

	std::filesystem::path root;
	std::filesystem::path bgroot;
};
