#ifndef __WP_LIB_PULSEAUDIO_H__
#define __WP_LIB_PULSEAUDIO_H__

typedef void wp_pulseaudio;

/**
 * @return A pulseaudio playback detector to be used
 */
wp_pulseaudio* wp_pulseaudio_create ();

/**
 * Frees any resources help up by the pulseaudio player
 *
 * @param pulseaudio
 */
void wp_pulseaudio_destroy (wp_pulseaudio* pulseaudio);

#endif