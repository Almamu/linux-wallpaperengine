#ifndef __WP_LIB_RENDER_H__
#define __WP_LIB_RENDER_H__

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "project.h"

/**
 * Renders a frame to the given framebuffer
 */
void WPENGINE_API wp_render_frame (wp_project* project);

/**
 * Updates time counters in the context so time "passes" on a background.
 *
 * This should be called once at the start of a full frame render
 *
 * @param context The context to update time for
 */
void WPENGINE_API wp_render_update_time (wp_context* context);

#ifdef __cplusplus
}
#endif

#endif