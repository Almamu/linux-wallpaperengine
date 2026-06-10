#ifndef __WP_LIB_PLAYLISTS_H__
#define __WP_LIB_PLAYLISTS_H__

#include "configuration.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

enum wp_playlist_mode {
    WP_PLAYLIST_MODE_UNKNOWN = -1,
    WP_PLAYLIST_MODE_TIMER = 0,
    WP_PLAYLIST_MODE_LOGON = 1,
    WP_PLAYLIST_MODE_DAYTIME = 2,
    WP_PLAYLIST_MODE_WEEKDAY = 3,
    WP_PLAYLIST_MODE_NEVER = 4,
    WP_PLAYLIST_MODE_MAX,
};

enum wp_playlist_order {
    WP_PLAYLIST_ORDER_UNKNOWN = -1,
    WP_PLAYLIST_ORDER_RANDOM = 0,
    WP_PLAYLIST_ORDER_SORTED = 1,
    WP_PLAYLIST_ORDER_MAX,
};

enum wp_playlist_transition {
    WP_PLAYLIST_TRANSITION_UNKNOWN = -1,
    WP_PLAYLIST_TRANSITION_NONE = 0, // none
    WP_PLAYLIST_TRANSITION_FADE = 1, // 0
    WP_PLAYLIST_TRANSITION_NONE_REDUCED_FLICKER = 2, // -2
    // random with no transitionpool is all,
    // otherwise the values are the same as the ones used to parse these
    WP_PLAYLIST_TRANSITION_RANDOM = 3, // random
    WP_PLAYLIST_TRANSITION_FADE_TO_BACK = 4, // 18
    WP_PLAYLIST_TRANSITION_MOSAIC = 5, // 1
    WP_PLAYLIST_TRANSITION_DIFFUSE = 6, // 2
    WP_PLAYLIST_TRANSITION_HORIZONTAL_SLIDE = 7, // 3
    WP_PLAYLIST_TRANSITION_VERTICAL_SLIDE = 8, // 4
    WP_PLAYLIST_TRANSITION_HORIZONTAL_FADE = 9, // 5
    WP_PLAYLIST_TRANSITION_VERTICAL_FADE = 10, // 6
    WP_PLAYLIST_TRANSITION_CLOUDS = 11, // 7
    WP_PLAYLIST_TRANSITION_BURN_PAPER = 12, // 8
    WP_PLAYLIST_TRANSITION_CIRCULAR = 13, // 9
    WP_PLAYLIST_TRANSITION_ZIPPER = 14, // 10
    WP_PLAYLIST_TRANSITION_DOOR = 15, // 11
    WP_PLAYLIST_TRANSITION_LINES = 16, // 12
    WP_PLAYLIST_TRANSITION_RADIAL_WIPE = 17, // 22
    WP_PLAYLIST_TRANSITION_ZOOM = 18, // 13
    WP_PLAYLIST_TRANSITION_TWISTER = 19, // 19
    WP_PLAYLIST_TRANSITION_DRIP = 20, // 14
    WP_PLAYLIST_TRANSITION_PIXELATE = 21, // 15
    WP_PLAYLIST_TRANSITION_BRICKS = 22, // 16
    WP_PLAYLIST_TRANSITION_PAINT = 23, // 17
    WP_PLAYLIST_TRANSITION_BLACK_HOLE = 24, // 20
    WP_PLAYLIST_TRANSITION_CRT = 25, // 21
    WP_PLAYLIST_TRANSITION_GLASS_SHATTER = 26, // 23
    WP_PLAYLIST_TRANSITION_BULLETS = 27, // 24
    WP_PLAYLIST_TRANSITION_ICE = 28, // 25
    WP_PLAYLIST_TRANSITION_BOILOVER = 29, // 26
    WP_PLAYLIST_TRANSITION_MAX,
};

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
    /**
     * Delay between playlist changes in minutes
     */
    int delay;
    /**
     * Transition time (in seconds)
     */
    int transitiontime;
    /**
     * The mode of the playlist
     */
    wp_playlist_mode mode;
    /**
     * The order of the playlist items
     */
    wp_playlist_order order;
    /**
     * The transition to use between items
     */
    wp_playlist_transition transition;
};

/**
 * Loads playlists based off the wp_configuration and gives access to them
 *
 * @param config The configuration instance
 * @return A pointer to the playlists context
 */
WPENGINE_API wp_playlists* wp_playlists_load (wp_configuration* config);

/**
 * WARNING: pointers returned by this function live as long as you do not reset or go to the next element.
 * if you want to keep this info for a UI or something, make copies of the data
 *
 * @return The next playlist entry in the list (if any) null otherwise
 */
WPENGINE_API wp_playlist_entry* wp_playlists_next (wp_playlists* playlists);

/**
 * @return Regardless of the current progress, restarts the playlist enumeration
 */
WPENGINE_API void wp_playlists_reset (wp_playlists* playlists);

/**
 * Destroys playlist information and cleans up used memory
 *
 * @param playlists The playlists context to destroy
 */
WPENGINE_API void wp_playlists_destroy (wp_playlists* playlists);

#ifdef __cplusplus
}
#endif

#endif