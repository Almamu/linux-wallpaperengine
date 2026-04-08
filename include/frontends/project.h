#ifndef __WP_LIB_PROJECT_H__
#define __WP_LIB_PROJECT_H__

#include "context.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Background project instance
 */
typedef void wp_project;

enum wp_mouse_input_button {
	WP_MOUSE_INPUT_BUTTON_LEFT = 1,
	wp_mouse_input_button_left = 1,
	WP_MOUSE_INPUT_BUTTON_RIGHT = 2,
	wp_mouse_input_button_right = 2,
	WP_MOUSE_INPUT_BUTTON_MIDDLE = 4,
	wp_mouse_input_button_middle = 4
};

/**
 * Property type
 */
enum wp_property_type {
	wp_property_type_unknown = 0,
	wp_property_type_text = 1,
	wp_property_type_slider = 2,
	wp_property_type_color = 3,
	wp_property_type_boolean = 4,
	wp_property_type_file = 4,
	wp_property_type_scene_texture = 6,
	wp_property_type_combo = 7,
};

/**
 * Provides callbacks to get mouse position
 */
struct wp_mouse_input {
	/**
	 * Pointer to user-defined data that will be passed to the callbacks
	 */
	void* user_parameter;
	/**
	 * Callback that returns the current mouse's x position on the given background
	 */
	double (*get_x) (void* user_parameter);
	/**
	 * Callback that returns the current mouse's y position on the given background
	 */
	double (*get_y) (void* user_parameter);
	/**
	 * Callback that returns the current mouse's button state
	 */
	int (*is_pressed) (void* user_parameter, int button);
};

/**
 * Project's property
 */
struct wp_project_property {
	wp_property_type type;
	const char* name;
	const char* text;
};

struct wp_project_property_slider {
	wp_project_property base;
	float min;
	float max;
	float step;
	float value;
};

struct wp_project_property_text {
	wp_project_property base;
	const char* value;
};

struct wp_project_property_file {
	wp_project_property base;
	const char* path;
};

struct wp_project_property_scene_texture {
	wp_project_property base;
	const char* value;
};

struct wp_project_property_combo_value {
	const char* key;
	const char* value;
};

struct wp_project_property_combo {
	wp_project_property base;
	int value_count;
	wp_project_property_combo_value* values;
};

struct wp_project_property_color {
	wp_project_property base;
	float r;
	float g;
	float b;
	float a;
};

struct wp_project_property_boolean {
	wp_project_property base;
	bool value;
};

/** Convenient macro to check for a property type */
#define WP_PROPERTY_IS(property, type)                                                                                 \
	(property != NULL && ((wp_project_property*)property)->ty##pe == wp_property_type_##type)
/** Convenient macro to cast a property to the given type safely */
#define WP_PROPERTY_AS(property, type) (WP_PROPERTY_IS (property, type) ? (wp_project_property_##type*)property : NULL)

#define WP_PROPERTY_IS_COLOR(property) WP_PROPERTY_IS (property, color)
#define WP_PROPERTY_AS_COLOR(property) WP_PROPERTY_AS (property, color)
#define WP_PROPERTY_IS_BOOLEAN(property) WP_PROPERTY_IS (property, boolean)
#define WP_PROPERTY_AS_BOOLEAN(property) WP_PROPERTY_AS (property, boolean)
#define WP_PROPERTY_IS_SLIDER(property) WP_PROPERTY_IS (property, slider)
#define WP_PROPERTY_AS_SLIDER(property) WP_PROPERTY_AS (property, slider)
#define WP_PROPERTY_IS_TEXT(property) WP_PROPERTY_IS (property, text)
#define WP_PROPERTY_AS_TEXT(property) WP_PROPERTY_AS (property, text)
#define WP_PROPERTY_IS_FILE(property) WP_PROPERTY_IS (property, file)
#define WP_PROPERTY_AS_FILE(property) WP_PROPERTY_AS (property, file)
#define WP_PROPERTY_IS_SCENE_TEXTURE(property) WP_PROPERTY_IS (property, scene_texture)
#define WP_PROPERTY_AS_SCENE_TEXTURE(property) WP_PROPERTY_AS (property, scene_texture)
#define WP_PROPERTY_IS_COMBO(property) WP_PROPERTY_IS (property, combo)
#define WP_PROPERTY_AS_COMBO(property) WP_PROPERTY_AS (property, combo)

/**
 * Loads the given background ID from the backgrounds folder
 *
 * @param context
 * @param mouse_input The mouse input handler if available
 * @param id The background id to load
 *
 * @return The project if was loaded successfully, NULL otherwise
 */
WPENGINE_API wp_project* wp_project_load_id (wp_context* context, wp_mouse_input* mouse_input, int id);

/**
 * Loads the given background ID from the backgrounds folder
 *
 * @param context
 * @param mouse_input The mouse input handler if available
 * @param id The background id to load
 *
 * @return The project if it was loaded successfully, NULL otherwise
 */
WPENGINE_API wp_project* wp_project_load_id_str (wp_context* context, wp_mouse_input* mouse_input, const char* id);

/**
 * Loads the project.json from the given folder
 *
 * @param context
 * @param mouse_input The mouse input handler if available
 * @param folder Full, absolute path to the background's project.json
 *
 * @return The project if was loaded successfully, NULL otherwise
 */
WPENGINE_API wp_project* wp_project_load_folder (wp_context* context, wp_mouse_input* mouse_input, const char* folder);

/**
 * WARNING: pointers returned by this function live as long as you do not reset, go to the next element or modify it.
 * if you want to keep this info for a UI or something, make copies of the data
 *
 * @param project The project to read properties off
 *
 * @return The current property entry with the available information
 */
WPENGINE_API wp_project_property* wp_project_property_list_next (wp_project* project);

/**
 * @param project Regardless of the current progress, restarts the property listing
 */
WPENGINE_API void wp_project_property_list_reset (wp_project* project);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_value over this one
 *
 * @param project
 * @param name
 * @param value
 */
WPENGINE_API bool wp_project_property_set_value_by_name (wp_project* project, const char* name, const char* value);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_text over this one
 *
 * @param project
 * @param name
 * @param value
 */
WPENGINE_API bool wp_project_property_set_text_by_name (wp_project* project, const char* name, const char* value);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_boolean over this one
 *
 * @param project
 * @param name
 * @param value
 */
WPENGINE_API bool wp_project_property_set_boolean_by_name (wp_project* project, const char* name, bool value);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_slider over this one
 *
 * @param project
 * @param name
 * @param value
 */
WPENGINE_API bool wp_project_property_set_slider_by_name (wp_project* project, const char* name, float value);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_color over this one
 *
 * @param project
 * @param name
 * @param r
 * @param g
 * @param b
 * @param a
 */
WPENGINE_API bool
wp_project_property_set_color_by_name (wp_project* project, const char* name, float r, float g, float b, float a);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_value over this one
 *
 * @param project
 * @param name
 * @param path
 */
WPENGINE_API bool wp_project_property_set_file_by_name (wp_project* project, const char* name, const char* path);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_value over this one
 *
 * @param project
 * @param name
 * @param path
 */
WPENGINE_API bool
wp_project_property_set_scene_texture_by_name (wp_project* project, const char* name, const char* path);

/**
 * WARNING: if the property being updated is the same as the last one returned from @see wp_project_property_list_next
 * the pointer will be freed and invalid.
 *
 * Prefer @see wp_project_property_set_value over this one
 *
 * @param project
 * @param name
 * @param key
 */
WPENGINE_API bool wp_project_property_set_combo_by_name (wp_project* project, const char* name, const char* key);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param value The value to set for the property
 */
WPENGINE_API bool wp_project_property_set_value (wp_project* project, wp_project_property* property, const char* value);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param value The value to set for the property
 */
WPENGINE_API bool
wp_project_property_set_text (wp_project* project, wp_project_property_text* property, const char* value);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param value The value to set for the property
 */
WPENGINE_API bool
wp_project_property_set_boolean (wp_project* project, wp_project_property_boolean* property, bool value);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param value The value to set for the property
 */
WPENGINE_API bool
wp_project_property_set_slider (wp_project* project, wp_project_property_slider* property, float value);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param r
 * @param g
 * @param b
 * @param a
 */
WPENGINE_API bool wp_project_property_set_color (
	wp_project* project, wp_project_property_color* property, float r, float g, float b, float a
);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param path The value to set for the property
 */
WPENGINE_API bool
wp_project_property_set_file (wp_project* project, wp_project_property_file* property, const char* path);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param path The value to set for the property
 */
WPENGINE_API bool wp_project_property_set_scene_texture (
	wp_project* project, wp_project_property_scene_texture* property, const char* path
);

/**
 * @param project The project to set the property for
 * @param property The property to update
 * @param key The value to set for the property
 */
WPENGINE_API bool
wp_project_property_set_combo (wp_project* project, wp_project_property_combo* property, const char* key);

/**
 * Frees any allocated memory by the project, stops the renderer (if setup)
 * and cleans up any used resources
 *
 * @param project The project to free
 */
WPENGINE_API void wp_project_destroy (wp_project* project);

/**
 * @param project The project to get render width for
 * @return The width of the project's render viewport
 */
WPENGINE_API int wp_project_get_width (wp_project* project);

/**
 * @param project The project to get render height for
 * @return The height of the project's render viewport
 */
WPENGINE_API int wp_project_get_height (wp_project* project);

/**
 * Updates the output framebuffer for the project
 *
 * @param project The project to update the output framebuffer for
 * @param framebuffer The framebuffer to use for rendering
 */
WPENGINE_API void wp_project_set_output_framebuffer (wp_project* project, unsigned int framebuffer);

#ifdef __cplusplus
}
#endif

#endif