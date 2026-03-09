#ifndef __WP_LIB_CONTEXT_H__
#define __WP_LIB_CONTEXT_H__

#include "configuration.h"

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
 * Sets up a context based on the given configuration and readies everything up for rendering
 * and loading backgrounds
 *
 * @param config The configuration to use for this context
 * @return The setup context
 */
wp_context* wp_context_create (const wp_configuration* config);

/**
 * Frees up any resources used by the context, stops the renderer, and cleans up anything used by it
 *
 * @param context The context to free
 */
void wp_context_destroy (wp_context* context);

/**
 * Updates the opengl function lookup address
 *
 * @param context The context to set the wp_gl_proc_address data for
 * @param address The address to use for opengl function lookups
 */
void wp_context_set_gl_proc_address (wp_context* context, wp_gl_proc_address* address);

#endif