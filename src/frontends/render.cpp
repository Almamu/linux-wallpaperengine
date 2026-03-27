#include "frontends/render.h"

#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Project.h"
#include "frontends/project.h"

#define WPENGINE_RENDER_API_BEGIN try {
#define WPENGINE_RENDER_API_END(result)                                                                                \
	}                                                                                                                  \
	catch (...) {                                                                                                      \
		return result;                                                                                                 \
	}

WPENGINE_API void wp_render_frame (wp_project* project) {
	WPENGINE_RENDER_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->render ();
	WPENGINE_RENDER_API_END ();
}

WPENGINE_API void wp_render_update_time (wp_context* context) {
	WPENGINE_RENDER_API_BEGIN
	const auto contextPtr = static_cast<WallpaperEngine::Context*> (context);

	contextPtr->renderTimeLast = contextPtr->renderTime;
	contextPtr->renderTime = contextPtr->time_counter->get_time (contextPtr->time_counter->user_parameter);
	WPENGINE_RENDER_API_END ();
}