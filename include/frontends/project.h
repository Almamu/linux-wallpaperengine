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
 * Loads the given background ID from the backgrounds folder
 *
 * @param context
 * @param mouse_input The mouse input handler if available
 * @param id The background id to load
 *
 * @return The project if was loaded successfully, NULL otherwise
 */
wp_project* WPENGINE_API wp_project_load_id (wp_context* context, wp_mouse_input* mouse_input, int id);

/**
 * Loads the project.json from the given folder
 *
 * @param context
 * @param mouse_input The mouse input handler if available
 * @param folder Full, absolute path to the background's project.json
 *
 * @return The project if was loaded successfully, NULL otherwise
 */
wp_project* WPENGINE_API wp_project_load_folder (wp_context* context, wp_mouse_input* mouse_input, const char* folder);

/**
 * Frees any allocated memory by the project, stops the renderer (if setup)
 * and cleans up any used resources
 *
 * @param project The project to free
 */
void WPENGINE_API wp_project_destroy (wp_project* project);

/**
 * @param project The project to get render width for
 * @return The width of the project's render viewport
 */
int WPENGINE_API wp_project_get_width (wp_project* project);

/**
 * @param project The project to get render height for
 * @return The height of the project's render viewport
 */
int WPENGINE_API wp_project_get_height (wp_project* project);

/**
 * Updates the output framebuffer for the project
 *
 * @param project The project to update the output framebuffer for
 * @param framebuffer The framebuffer to use for rendering
 */
void WPENGINE_API wp_project_set_output_framebuffer (wp_project* project, unsigned int framebuffer);

#ifdef __cplusplus
}
#endif

#endif