#ifndef __WP_LIB_CONTENT_H__
#define __WP_LIB_CONTENT_H__

#include "configuration.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Background list operation
 */
typedef void wp_background_list;

/**
 * Background list entry
 */
struct wp_background_list_entry {
	const char* path;
	const char* preview_path;
};

/**
 * @param config The configuration to use to discover backgrounds
 * @return The listing process instance or nullptr if no background_dir is set
 */
WPENGINE_API wp_background_list* wp_background_list_open (wp_configuration* config);

/**
 * WARNING: pointers returned by this function live as long as you do not reset or go to the next element.
 * if you want to keep this info for a UI or something, make copies of the data
 *
 * @param list The list to iterate through
 * @return The next background's info or nullptr if an error/eof is found
 */
WPENGINE_API wp_background_list_entry* wp_background_list_next (wp_background_list* list);

/**
 * @return Regardless of the current progress, restarts the background enumeration
 */
WPENGINE_API void wp_background_list_reset (wp_background_list* list);

/**
 * @param list The list to close and free resources for
 */
WPENGINE_API void wp_background_list_close (wp_background_list* list);

#ifdef __cplusplus
}
#endif

#endif
