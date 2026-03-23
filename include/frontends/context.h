#ifndef __WP_LIB_CONTEXT_H__
#define __WP_LIB_CONTEXT_H__

#include "export.h"
#include "configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Context instance
 */
typedef void wp_context;

/**
 * Provides callbacks to get opengl methods to call by the renderer
 */
struct wp_gl_proc_address {
	/**
	 * Pointer to user-defined data that will be passed to the callbacks
	 */
	void* user_parameter;

	/**
	 * Requests the address of the specified opengl function
	 */
	void* (*get_proc_address) (void* user_parameter, const char* name);
};

/**
 * Provides callbacks to the timing system
 */
struct wp_time_counter {
	/**
	 * Pointer to user-defined data that will be passed to the callbacks
	 */
	void* user_parameter;

	/**
	 * Requests the current time
	 */
	float (*get_time) (void* user_parameter);
};

/**
 * Sets up a context based on the given configuration and readies everything up for rendering
 * and loading backgrounds
 *
 * @param config The configuration to use for this context
 * @return The setup context
 */
WPENGINE_API wp_context* wp_context_create (const wp_configuration* config);

/**
 * Frees up any resources used by the context, stops the renderer, and cleans up anything used by it
 *
 * @param context The context to free
 */
WPENGINE_API void wp_context_destroy (wp_context* context);

/**
 * Updates the opengl function lookup address
 *
 * @param context The context to set the wp_gl_proc_address data for
 * @param address The address to use for opengl function lookups
 */
WPENGINE_API void wp_context_set_gl_proc_address (wp_context* context, wp_gl_proc_address* address);

/**
 * Updates the time counter handlers
 *
 * @param context The context to set the wp_time_counter for
 * @param counter The new counter to use in this context
 */
WPENGINE_API void wp_context_set_time_counter (wp_context* context, wp_time_counter* counter);

#ifdef __cplusplus
}
#endif

#endif