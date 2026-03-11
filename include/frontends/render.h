#ifndef __WP_LIB_RENDER_H__
#define __WP_LIB_RENDER_H__

#include "project.h"

/**
 * Renders a frame to the given framebuffer
 */
void wp_render_frame (wp_project* project);

/**
 * Updates time counters in the context so time "passes" on a background.
 *
 * This should be called once at the start of a full frame render
 *
 * @param context The context to update time for
 */
void wp_render_update_time (wp_context* context);

#endif