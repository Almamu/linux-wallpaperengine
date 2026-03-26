#include "frontends/playlists.h"

#include "WallpaperEngine/Configuration.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Parsers/ConfigParser.h"

#define WPENGINE_PLAYLIST_API_BEGIN try {
#define WPENGINE_PLAYLIST_API_END(result)                                                                              \
	}                                                                                                                  \
	catch (...) {                                                                                                      \
		return result;                                                                                                 \
	}

wp_playlists* wp_playlists_load (wp_configuration* config) {
	WPENGINE_PLAYLIST_API_BEGIN
	return static_cast<WallpaperEngine::Configuration*> (config)->openPlaylistList ();
	WPENGINE_PLAYLIST_API_END (nullptr);
}

wp_playlist_entry* wp_playlists_next (wp_playlists* playlists) {
	WPENGINE_PLAYLIST_API_BEGIN
	return static_cast<WallpaperEngine::PlaylistListEntry*> (playlists)->next ();
	WPENGINE_PLAYLIST_API_END (nullptr);
}

void wp_playlists_destroy (wp_playlists* playlists) {
	WPENGINE_PLAYLIST_API_BEGIN
	delete static_cast<WallpaperEngine::PlaylistListEntry*> (playlists);
	WPENGINE_PLAYLIST_API_END ();
}