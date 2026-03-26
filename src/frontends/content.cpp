#include "frontends/content.h"

#include "WallpaperEngine/Configuration.h"

#define WPENGINE_CONTENT_API_BEGIN try {
#define WPENGINE_CONTENT_API_END(result) } catch (...) { \
	return result; \
}

wp_background_list* wp_background_list_open (wp_configuration* config) {
	WPENGINE_CONTENT_API_BEGIN
	return static_cast<WallpaperEngine::Configuration*> (config)->openBackgroundList ();
	WPENGINE_CONTENT_API_END(nullptr)
}

wp_background_list_entry* wp_background_list_next (wp_background_list* list) {
	WPENGINE_CONTENT_API_BEGIN
	return  static_cast<WallpaperEngine::ContentListEntry*> (list)->next ();;
	WPENGINE_CONTENT_API_END(nullptr)
}

void wp_background_list_close (wp_background_list* list) {
	delete static_cast<WallpaperEngine::ContentListEntry*> (list);
}