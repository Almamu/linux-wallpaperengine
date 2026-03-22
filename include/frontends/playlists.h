#ifndef __WP_LIB_PLAYLISTS_H__
#define __WP_LIB_PLAYLISTS_H__

#include "configuration.h"

/**
 * Playlists context instance
 */
typedef void wp_playlists;

/**
 * Information of a specific playlist entry
 */
struct wp_playlist_entry {
	/**
	 * Playlist name
	 */
	const char* name;
	/**
	 * The number of items available in this playlist
	 */
	int item_count;
	/**
	 * Backgrounds available in the playlist
	 */
	const char** items;
	/**
	 * End of the daytime for the background to change
	 */
	float* daytimeend;
};

/**
 * Loads playlists based off the wp_configuration and gives access to them
 *
 * @param config The configuration instance
 * @return A pointer to the playlists context
 */
wp_playlists* wp_playlists_load (wp_configuration* config);

/**
 * @return The next playlist entry in the list (if any) null otherwise
 */
wp_playlist_entry* wp_playlists_next (wp_playlists* playlists);

/**
 * Destroys playlist information and cleans up used memory
 *
 * @param playlists The playlists context to destroy
 */
void wp_playlists_destroy (wp_playlists* playlists);

#endif