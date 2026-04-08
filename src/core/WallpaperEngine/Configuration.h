#pragma once

#include "Data/Model/Types.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "frontends/configuration.h"
#include "frontends/content.h"
#include "frontends/playlists.h"

namespace WallpaperEngine {
class Configuration;

class PlaylistListEntry {
public:
	Data::Model::ConfigUniquePtr config;
	Data::Model::PlaylistMap::iterator it;

	wp_playlist_entry entry;

	~PlaylistListEntry ();
	wp_playlist_entry* next ();
	void reset ();
};

class ContentListEntry {
public:
	std::filesystem::directory_iterator it;
	Data::Model::Properties::const_iterator property_it;
	std::optional<std::filesystem::path> current_path;
	std::optional<std::filesystem::path> preview_path;
	Configuration* config;

	wp_background_list_entry current_entry;

	wp_background_list_entry* next ();
	void reset ();
};

class Configuration {
public:
	std::map<std::string, std::string> properties;
	wp_rendering_pause_check* pause_check;
	wp_mute_check* mute_check;
	int volume;
	bool enableAudio;
	bool disableParticles;
	bool disableParallax;
	int web_fps;

	std::filesystem::path assets_dir;
	std::filesystem::path backgrounds_dir;

	Configuration (wp_rendering_pause_check* pause_check, wp_mute_check* mute_check);
	~Configuration () = default;

	bool detectSteamDir ();
	bool setSteamDir (const std::filesystem::path& path);
	bool setAssetsDir (const std::filesystem::path& path);
	bool setBackgroundsDir (const std::filesystem::path& path);
	ContentListEntry* openBackgroundList ();
	PlaylistListEntry* openPlaylistList ();
};
}