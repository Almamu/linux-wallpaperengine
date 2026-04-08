#include "frontends/content.h"

#include "WallpaperEngine/Configuration.h"

#define WPENGINE_CONTENT_API_BEGIN try {
#define WPENGINE_CONTENT_API_END(result)                                                                               \
	}                                                                                                                  \
	catch (...) {                                                                                                      \
		return result;                                                                                                 \
	}

WPENGINE_API wp_background_list* wp_background_list_open (wp_configuration* config) {
	WPENGINE_CONTENT_API_BEGIN
	return static_cast<WallpaperEngine::Configuration*> (config)->openBackgroundList ();
	WPENGINE_CONTENT_API_END (nullptr)
}

WPENGINE_API wp_background_list_entry* wp_background_list_next (wp_background_list* list) {
	WPENGINE_CONTENT_API_BEGIN
	return static_cast<WallpaperEngine::ContentListEntry*> (list)->next ();
	WPENGINE_CONTENT_API_END (nullptr)
}

WPENGINE_API void wp_background_list_reset (wp_background_list* list) {
	WPENGINE_CONTENT_API_BEGIN
	static_cast<WallpaperEngine::ContentListEntry*> (list)->reset ();
	WPENGINE_CONTENT_API_END ()
}

WPENGINE_API void wp_background_list_close (wp_background_list* list) {
	delete static_cast<WallpaperEngine::ContentListEntry*> (list);
}