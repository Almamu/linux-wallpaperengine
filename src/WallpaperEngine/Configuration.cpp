#include <algorithm>
#include <vector>
#include <fstream>

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"
#include "Configuration.h"

#include "Data/Parsers/ConfigParser.h"

using namespace WallpaperEngine;

std::vector<std::string> steam_paths = {
	".steam/steam",
	".local/share/Steam",
	".var/app/com.valvesoftware.Steam/.local/share/Steam",
	"snap/steam/common/.local/share/Steam",
};

std::filesystem::path detectHomedir () {
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

Configuration::Configuration (wp_rendering_pause_check* pause_check, wp_mute_check* mute_check) :
	properties ({}),
	pause_check (pause_check),
	mute_check (mute_check),
	volume (15),
	enableAudio (true),
	disableParticles (false),
	disableParallax (false),
	web_fps (60) {
	this->detectSteamDir ();
}

bool Configuration::detectSteamDir () {
	const std::filesystem::path homedir = detectHomedir ();

	return std::ranges::any_of (
		steam_paths.begin (), steam_paths.end (), [this, homedir] (const std::string& path) -> bool {
			const std::filesystem::path real_path = homedir / path;

			return this->setSteamDir (real_path);
		}
	);
}

bool Configuration::setSteamDir (const std::filesystem::path& path) {
	// check for workshop and content folders
	try {
		const std::filesystem::path backgrounds = Steam::FileSystem::workshopDirectory (path, WORKSHOP_APP_ID);
		const std::filesystem::path assets = Steam::FileSystem::appDirectory (path, "wallpaper_engine") / "assets";

		// prevent partially setting these values as they may break
		if (!std::filesystem::exists (backgrounds) || !std::filesystem::exists (backgrounds)) {
			return false;
		}

		if (!std::filesystem::exists (assets) || !std::filesystem::is_directory (assets)) {
			return false;
		}

		this->backgrounds_dir = backgrounds;
		this->assets_dir = assets;

		return true;
	} catch (std::runtime_error&) {
		return false;
	}
}

bool Configuration::setAssetsDir (const std::filesystem::path& path) {
	if (!std::filesystem::exists (path) || !std::filesystem::is_directory (path)) {
		return false;
	}

	this->assets_dir = path;

	return true;
}

bool Configuration::setBackgroundsDir (const std::filesystem::path& path) {
	if (!std::filesystem::exists (path) || !std::filesystem::is_directory (path)) {
		return false;
	}

	this->backgrounds_dir = path;

	return true;
}

ContentListEntry* Configuration::openBackgroundList () {
	if (this->backgrounds_dir.empty ()) {
		return nullptr;
	}

	if (!std::filesystem::exists (this->backgrounds_dir)) {
		return nullptr;
	}

	if (!std::filesystem::is_directory (this->backgrounds_dir)) {
		return nullptr;
	}

	// open directory and allocate structs
	return new ContentListEntry {
		.it = std::filesystem::directory_iterator (this->backgrounds_dir),
		.config = this,
		.current_entry = { .path = nullptr, .preview_path = nullptr },
	};
}

PlaylistListEntry* Configuration::openPlaylistList () {
	auto baseConfig = WallpaperEngine::Data::Parsers::ConfigParser::load (
		this->assets_dir / ".." / "config.json"
	);
	const auto it = baseConfig->playlists.begin ();

	return new PlaylistListEntry {
		.config = std::move (baseConfig),
		.it = it,
		.entry = {
			.name = nullptr,
			.items = nullptr
		}
	};
}

wp_background_list_entry* ContentListEntry::next () {
	do {
		if (this->it == std::filesystem::directory_iterator ()) {
			// also update the entry to ensure it points to nothing
			this->current_path = std::nullopt;
			this->preview_path = std::nullopt;

			// reset the iterator so the next call starts off the beginning again
			this->it = std::filesystem::directory_iterator (this->config->backgrounds_dir);

			break;
		}

		if (!this->it->is_directory ()) {
			continue;
		}

		this->current_path = this->it->path ();
		++this->it;
		break;
	} while (true);

	if (this->current_path.has_value ()) {
		// open the project.json file, get the preview, and return the contents
		// no need to use the full-blown parser just for this

		// parse file off the json directly
		const auto json = WallpaperEngine::Data::Parsers::JSON::parse (
			std::ifstream (this->current_path.value () / "project.json")
		);

		if (const auto preview = json.optional ("preview"); preview.has_value ()) {
			this->preview_path = this->current_path.value () / preview.value ();
		} else {
			this->preview_path = std::nullopt;
		}
	}

	if (this->current_path.has_value ()) {
		this->current_entry.path = this->current_path.value ().c_str ();
	} else {
		this->current_entry.path = nullptr;
	}

	if (this->preview_path.has_value ()) {
		this->current_entry.preview_path = this->preview_path.value ().c_str ();
	} else {
		this->current_entry.preview_path = nullptr;
	}

	if (this->current_path.has_value () || this->preview_path.has_value ()) {
		return &this->current_entry;
	}

	return nullptr;
}

PlaylistListEntry::~PlaylistListEntry () {
	this->entry.name = nullptr;
	delete [] this->entry.items;
	delete [] this->entry.daytimeend;
	this->entry.item_count = 0;
	this->entry.items = nullptr;
	this->entry.daytimeend = nullptr;
}

wp_playlist_entry* PlaylistListEntry::next () {
	if (this->it == this->config->playlists.end ()) {
		// reset the iterator so the next call starts off the beginning
		this->it = this->config->playlists.begin ();

		return nullptr;
	}

	// free previous list
	delete[] this->entry.items;
	delete[] this->entry.daytimeend;

	// iterator is valid otherwise, extract important information and place it in the allocated structure
	this->entry.name = this->it->first.c_str ();
	this->entry.item_count = this->it->second->items.size ();
	this->entry.items = new const char*[this->entry.item_count];
	this->entry.daytimeend = new float[this->entry.item_count];

	size_t i = 0;

	for (const auto& item : this->it->second->items) {
		this->entry.items[i] = item->path.c_str ();
		this->entry.daytimeend[i++] = item->daytimeend;
	}

	++this->it;

	// finally return the playlist entry to the caller
	return &this->entry;
}