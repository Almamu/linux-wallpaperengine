#pragma once

#include "ApplicationContext.h"

namespace WallpaperEngine::Application {
/**
 * Represents current application state
 */
class ApplicationState {
public:
    struct {
	bool keepRunning;
    } general {};

    struct {
	bool enabled;
	int volume;
    } audio {};

    struct {
	bool enabled;
    } mouse {};

    struct {
	/**
	 * Frame rate the current wallpapers actually produce, 0 when they need the full --fps budget
	 * (anything but plain video wallpapers, or a video whose rate isn't known yet)
	 */
	float contentFrameRate;
    } render {};
};
} // namespace WallpaperEngine::Application