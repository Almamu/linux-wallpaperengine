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
    WP_MOUSE_INPUT_BUTTON_RIGHT = 2,
    WP_MOUSE_INPUT_BUTTON_MIDDLE = 4,
};

/**
 * Property type
 */
enum wp_property_type {
    WP_PROPERTY_TYPE_UNKNOWN = 0,
    WP_PROPERTY_TYPE_TEXT = 1,
    WP_PROPERTY_TYPE_SLIDER = 2,
    WP_PROPERTY_TYPE_COLOR = 3,
    WP_PROPERTY_TYPE_BOOLEAN = 4,
    WP_PROPERTY_TYPE_FILE = 4,
    WP_PROPERTY_TYPE_SCENE_TEXTURE = 6,
    WP_PROPERTY_TYPE_COMBO = 7,
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
 * Provides callbacks to describe the project's contents for debugging purposes
 */
struct wp_describe_callback {
    /**
     * Pointer to user-defined data that will be passed to the callbacks
     */
    void* user_parameter;

    /**
     * Called by the describe function with data to write to the output buffer.
     */
    void (*write) (void* user_parameter, const char* buffer, unsigned long size);
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

#define WP_TYPE_LOWER_BOOLEAN boolean
#define WP_TYPE_LOWER_COLOR color
#define WP_TYPE_LOWER_SLIDER slider
#define WP_TYPE_LOWER_TEXT text
#define WP_TYPE_LOWER_FILE file
#define WP_TYPE_LOWER_SCENE_TEXTURE scene_texture
#define WP_TYPE_LOWER_COMBO combo

#define WP_TYPE_EXPAND(x) x
#define WP_TYPE_CONCAT(a, b) a##b
#define WP_TYPE_CONCAT_EXPAND(a, b) WP_TYPE_CONCAT (a, b)

#define WP_PROPERTY_TYPE_NAME(type)                                                                                    \
    WP_TYPE_CONCAT_EXPAND (wp_project_property_, WP_TYPE_EXPAND (WP_TYPE_CONCAT_EXPAND (WP_TYPE_LOWER_, type)))

/** Convenient macro to check for a property type */
#define WP_PROPERTY_IS(property, type)                                                                                 \
    (property != NULL && ((wp_project_property*)property)->ty##pe == WP_PROPERTY_TYPE_##type)
/** Convenient macro to cast a property to the given type safely */
#define WP_PROPERTY_AS(property, type)                                                                                 \
    (WP_PROPERTY_IS (property, type) ? (WP_PROPERTY_TYPE_NAME (type)*)property : NULL)

#define WP_PROPERTY_IS_COLOR(property) WP_PROPERTY_IS (property, COLOR)
#define WP_PROPERTY_AS_COLOR(property) WP_PROPERTY_AS (property, COLOR)
#define WP_PROPERTY_IS_BOOLEAN(property) WP_PROPERTY_IS (property, BOOLEAN)
#define WP_PROPERTY_AS_BOOLEAN(property) WP_PROPERTY_AS (property, BOOLEAN)
#define WP_PROPERTY_IS_SLIDER(property) WP_PROPERTY_IS (property, SLIDER)
#define WP_PROPERTY_AS_SLIDER(property) WP_PROPERTY_AS (property, SLIDER)
#define WP_PROPERTY_IS_TEXT(property) WP_PROPERTY_IS (property, TEXT)
#define WP_PROPERTY_AS_TEXT(property) WP_PROPERTY_AS (property, TEXT)
#define WP_PROPERTY_IS_FILE(property) WP_PROPERTY_IS (property, FILE)
#define WP_PROPERTY_AS_FILE(property) WP_PROPERTY_AS (property, FILE)
#define WP_PROPERTY_IS_SCENE_TEXTURE(property) WP_PROPERTY_IS (property, SCENE_TEXTURE)
#define WP_PROPERTY_AS_SCENE_TEXTURE(property) WP_PROPERTY_AS (property, SCENE_TEXTURE)
#define WP_PROPERTY_IS_COMBO(property) WP_PROPERTY_IS (property, COMBO)
#define WP_PROPERTY_AS_COMBO(property) WP_PROPERTY_AS (property, COMBO)

/**
 * Loads the given background ID from the backgrounds folder
 *
 * @param context
 * @param mouse_input The mouse input handler if available
 * @param id The background id to load
 *
 * @return The project if was loaded successfully, NULL otherwise
 */
WPENGINE_API wp_project* wp_project_load_id (wp_context* context, wp_mouse_input* mouse_input, unsigned long int id);

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
 * @return The width of the project's render viewport. This can change at any point
 */
WPENGINE_API int wp_project_get_width (wp_project* project);

/**
 * @param project The project to get render height for
 * @return The height of the project's render viewport. This can change at any point
 */
WPENGINE_API int wp_project_get_height (wp_project* project);

/**
 * Gives the project a hint for the size it will render at
 *
 * @param project The project to hint the size for
 * @param width The width to hint for the project
 * @param height The height to hint for the project
 */
WPENGINE_API void wp_project_hint_size (wp_project* project, int width, int height);

/**
 * Updates the output framebuffer for the project
 *
 * @param project The project to update the output framebuffer for
 * @param framebuffer The framebuffer to use for rendering
 */
WPENGINE_API void wp_project_set_output_framebuffer (wp_project* project, unsigned int framebuffer);

/**
 * Updates the mouse input used for the project
 *
 * @param project The project to update mouse input handler for
 * @param mouse_input The mouse input handler to use for the project
 */
WPENGINE_API void wp_project_set_mouse_input (wp_project* project, wp_mouse_input* mouse_input);

/**
 * Provides a textual representation of the project and it's elements, useful for debugging
 *
 * @param project
 * @param callback
 */
WPENGINE_API void wp_project_describe (wp_project* project, wp_describe_callback* callback);

/**
 * Notifies the project about a new track being played in the system
 *
 * @param title
 * @param artist
 * @param album
 */
WPENGINE_API void
wp_project_notify_track_metadata_change (wp_project* project, const char* title, const char* artist, const char* album);

/**
 * Notifies the project about a new album art url being available
 *
 * @param project
 * @param url
 */
WPENGINE_API void wp_project_notify_album_art_url_change (wp_project* project, const char* url);

/**
 * Notifies the project about playback state change
 *
 * @param project
 * @param state
 */
WPENGINE_API void wp_project_notify_playback_state_change (wp_project* project, wp_media_playback_state state);

/**
 * Notifies the project about changes in the playback position
 *
 * @param project
 * @param position
 */
WPENGINE_API void
wp_project_notify_playback_position_and_duration_change (wp_project* project, double position, double duration);

#ifdef __cplusplus
}
#endif

#endif