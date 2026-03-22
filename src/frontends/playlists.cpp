#include "frontends/playlists.h"

#include "WallpaperEngine/Configuration.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Parsers/ConfigParser.h"

struct wp_playlists_impl {
	WallpaperEngine::Data::Model::ConfigUniquePtr config;
	WallpaperEngine::Data::Model::PlaylistMap::iterator it;

	wp_playlist_entry entry;
};

wp_playlists* wp_playlists_load (wp_configuration* config) {
	try {
		auto baseConfig = WallpaperEngine::Data::Parsers::ConfigParser::load (
			static_cast<WallpaperEngine::Configuration*> (config)->assets_dir / ".." / "config.json"
		);
		const auto it = baseConfig->playlists.begin ();

		return new wp_playlists_impl { .config = std::move (baseConfig),
			                           .it = it,
			                           .entry = { .name = nullptr, .items = nullptr } };
	} catch (...) {
		return nullptr;
	}
}

wp_playlist_entry* wp_playlists_next (wp_playlists* playlists) {
	const auto playlistPtr = static_cast<wp_playlists_impl*> (playlists);

	if (playlistPtr->it == playlistPtr->config->playlists.end ()) {
		// reset the iterator so the next call starts off the beginning
		playlistPtr->it = playlistPtr->config->playlists.begin ();

		return nullptr;
	}

	// free previous list
	delete[] playlistPtr->entry.items;
	delete[] playlistPtr->entry.daytimeend;

	// iterator is valid otherwise, extract important information and place it in the allocated structure
	playlistPtr->entry.name = playlistPtr->it->first.c_str ();
	playlistPtr->entry.item_count = playlistPtr->it->second->items.size ();
	playlistPtr->entry.items = new const char*[playlistPtr->entry.item_count];
	playlistPtr->entry.daytimeend = new float[playlistPtr->entry.item_count];

	size_t i = 0;

	for (const auto& item : playlistPtr->it->second->items) {
		playlistPtr->entry.items[i] = item->path.c_str ();
		playlistPtr->entry.daytimeend[i++] = item->daytimeend;
	}

	++playlistPtr->it;

	// finally return the playlist entry to the caller
	return &playlistPtr->entry;
}

void wp_playlists_destroy (wp_playlists* playlists) {
	const auto playlistPtr = static_cast<wp_playlists_impl*> (playlists);

	playlistPtr->entry.name = nullptr;
	delete[] playlistPtr->entry.items;
	delete[] playlistPtr->entry.daytimeend;
	playlistPtr->entry.item_count = 0;
	playlistPtr->entry.items = nullptr;
	playlistPtr->entry.daytimeend = nullptr;

	delete playlistPtr;
}