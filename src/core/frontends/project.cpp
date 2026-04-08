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

WPENGINE_API wp_project* wp_project_load_id_str (wp_context* context, wp_mouse_input* mouse_input, const char* id) {
	WPENGINE_PROJECT_API_BEGIN
	return WallpaperEngine::Project::loadId (static_cast<WallpaperEngine::Context*> (context), mouse_input, id);
	WPENGINE_PROJECT_API_END (nullptr);
}

WPENGINE_API wp_project* wp_project_load_folder (wp_context* context, wp_mouse_input* mouse_input, const char* folder) {
	WPENGINE_PROJECT_API_BEGIN
	return WallpaperEngine::Project::loadFolder (static_cast<WallpaperEngine::Context*> (context), mouse_input, folder);
	WPENGINE_PROJECT_API_END (nullptr);
}
WPENGINE_API wp_project_property* wp_project_property_list_next (wp_project* project) {
	WPENGINE_PROJECT_API_BEGIN
	return static_cast<WallpaperEngine::Project*> (project)->propertyListNext ();
	WPENGINE_PROJECT_API_END (nullptr);
}

WPENGINE_API void wp_project_property_list_reset (wp_project* project) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertyListReset ();
	WPENGINE_PROJECT_API_END ();
}

WPENGINE_API bool wp_project_property_set_value_by_name (wp_project* project, const char* name, const char* value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool wp_project_property_set_text_by_name (wp_project* project, const char* name, const char* value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool wp_project_property_set_boolean_by_name (wp_project* project, const char* name, bool value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool wp_project_property_set_slider_by_name (wp_project* project, const char* name, float value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_color_by_name (wp_project* project, const char* name, float r, float g, float b, float a) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, glm::vec4 (r, g, b, a));
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool wp_project_property_set_file_by_name (wp_project* project, const char* name, const char* path) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, path);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_scene_texture_by_name (wp_project* project, const char* name, const char* path) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, path);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool wp_project_property_set_combo_by_name (wp_project* project, const char* name, const char* key) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (name, key);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_value (wp_project* project, wp_project_property* property, const char* value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (property, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_text (wp_project* project, wp_project_property_text* property, const char* value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (&property->base, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_boolean (wp_project* project, wp_project_property_boolean* property, bool value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (&property->base, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_slider (wp_project* project, wp_project_property_slider* property, float value) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (&property->base, value);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool wp_project_property_set_color (
	wp_project* project, wp_project_property_color* property, float r, float g, float b, float a
) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (&property->base, glm::vec4 (r, g, b, a));
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_file (wp_project* project, wp_project_property_file* property, const char* path) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (&property->base, path);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool wp_project_property_set_scene_texture (
	wp_project* project, wp_project_property_scene_texture* property, const char* path
) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (&property->base, path);
	return true;
	WPENGINE_PROJECT_API_END (false);
}

WPENGINE_API bool
wp_project_property_set_combo (wp_project* project, wp_project_property_combo* property, const char* key) {
	WPENGINE_PROJECT_API_BEGIN
	static_cast<WallpaperEngine::Project*> (project)->propertySet (&property->base, key);
	return true;
	WPENGINE_PROJECT_API_END (false);
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