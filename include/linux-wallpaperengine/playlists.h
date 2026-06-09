#ifndef __WP_LIB_PLAYLISTS_H__
#define __WP_LIB_PLAYLISTS_H__

#include "configuration.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

enum wp_playlist_mode {
	wp_playlist_mode_Unknown = -1,
	wp_playlist_mode_Timer = 0,
	wp_playlist_mode_Logon = 1,
	wp_playlist_mode_Daytime = 2,
	wp_playlist_mode_Weekday = 3,
	wp_playlist_mode_Never = 4,
	wp_playlist_mode_Max,
};

enum wp_playlist_order {
	wp_playlist_order_Unknown = -1,
	wp_playlist_order_Random = 0,
	wp_playlist_order_Sorted = 1,
	wp_playlist_order_Max,
};

enum wp_playlist_transition {
	wp_playlist_transition_Unknown = -1,
	wp_playlist_transition_None = 0, // none
	wp_playlist_transition_Fade = 1, // 0
	wp_playlist_transition_NoneReducedFlicker = 2, // -2
	// random with no transitionpool is all,
	// otherwise the values are the same as the ones used to parse these
	wp_playlist_transition_Random = 3, // random
	wp_playlist_transition_FadeToBlack = 4, // 18
	wp_playlist_transition_Mosaic = 5, // 1
	wp_playlist_transition_Diffuse = 6, // 2
	wp_playlist_transition_HorizontalSlide = 7, // 3
	wp_playlist_transition_VerticalSlide = 8, // 4
	wp_playlist_transition_HorizontalFade = 9, // 5
	wp_playlist_transition_VerticalFade = 10, // 6
	wp_playlist_transition_Clouds = 11, // 7
	wp_playlist_transition_BurnPaper = 12, // 8
	wp_playlist_transition_Circular = 13, // 9
	wp_playlist_transition_Zipper = 14, // 10
	wp_playlist_transition_Door = 15, // 11
	wp_playlist_transition_Lines = 16, // 12
	wp_playlist_transition_RadialWipe = 17, // 22
	wp_playlist_transition_Zoom = 18, // 13
	wp_playlist_transition_Twister = 19, // 19
	wp_playlist_transition_Drip = 20, // 14
	wp_playlist_transition_Pixelate = 21, // 15
	wp_playlist_transition_Bricks = 22, // 16
	wp_playlist_transition_Paint = 23, // 17
	wp_playlist_transition_BlackHole = 24, // 20
	wp_playlist_transition_CRT = 25, // 21
	wp_playlist_transition_GlassShatter = 26, // 23
	wp_playlist_transition_Bullets = 27, // 24
	wp_playlist_transition_Ice = 28, // 25
	wp_playlist_transition_Boilover = 29, // 26
	wp_playlist_transition_Max,
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