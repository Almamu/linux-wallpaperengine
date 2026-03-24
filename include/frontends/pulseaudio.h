#ifndef __WP_LIB_PULSEAUDIO_H__
#define __WP_LIB_PULSEAUDIO_H__

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void wp_pulseaudio;

/**
 * @return A pulseaudio playback detector to be used
 */
wp_pulseaudio* WPENGINE_API wp_pulseaudio_create ();

/**
 * Frees any resources help up by the pulseaudio player
 *
 * @param pulseaudio
 */
void WPENGINE_API wp_pulseaudio_destroy (wp_pulseaudio* pulseaudio);

#ifdef __cplusplus
}
#endif

#endif