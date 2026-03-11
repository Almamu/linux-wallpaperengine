#include "frontends/render.h"

#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "frontends/project.h"

void wp_render_frame (wp_project* project) {
	static_cast<WallpaperEngine::LoadedProject*> (project)->wallpaper->render ();
}

void wp_render_update_time (wp_context* context) {
	const auto contextPtr = static_cast<WallpaperEngine::Context*> (context);

	contextPtr->renderTimeLast = contextPtr->renderTime;
	contextPtr->renderTime = contextPtr->time_counter->get_time (contextPtr->time_counter->user_parameter);
}