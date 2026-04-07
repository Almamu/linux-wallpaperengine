#include "frontends/project.h"

#include "WallpaperEngine/Project.h"

#define WPENGINE_PROJECT_API_BEGIN try {
#define WPENGINE_PROJECT_API_END(result)                                                                               \
	}                                                                                                                  \
	catch (...) {                                                                                                      \
		return result;                                                                                                 \
	}

WPENGINE_API wp_project* wp_project_load_id (wp_context* context, wp_mouse_input* mouse_input, const int id) {
	WPENGINE_PROJECT_API_BEGIN
	return WallpaperEngine::Project::loadId (static_cast<WallpaperEngine::Context*> (context), mouse_input, id);
	WPENGINE_PROJECT_API_END (nullptr);
}

WPENGINE_API wp_project* wp_project_load_folder (wp_context* context, wp_mouse_input* mouse_input, const char* folder) {
	WPENGINE_PROJECT_API_BEGIN
	return WallpaperEngine::Project::loadFolder (static_cast<WallpaperEngine::Context*> (context), mouse_input, folder);
	WPENGINE_PROJECT_API_END (nullptr);
}

WPENGINE_API void wp_project_destroy (wp_project* project) {
	WPENGINE_PROJECT_API_BEGIN
	delete static_cast<WallpaperEngine::Project*> (project);
	WPENGINE_PROJECT_API_END ();
}

WPENGINE_API int wp_project_get_width (wp_project* project) {
	WPENGINE_PROJECT_API_BEGIN
	return static_cast<WallpaperEngine::Project*> (project)->getWidth ();
	WPENGINE_PROJECT_API_END (0);
}

WPENGINE_API int wp_project_get_height (wp_project* project) {
	WPENGINE_PROJECT_API_BEGIN
	return static_cast<WallpaperEngine::Project*> (project)->getHeight ();
	WPENGINE_PROJECT_API_END (0);
}

WPENGINE_API void wp_project_set_output_framebuffer (wp_project* project, const unsigned int framebuffer) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->setOutputFramebuffer (framebuffer);
	WPENGINE_PROJECT_API_END ();
}