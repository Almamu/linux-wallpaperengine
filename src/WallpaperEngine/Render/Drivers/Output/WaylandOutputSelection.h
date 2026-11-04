#pragma once

// Bookkeeping for Wayland outputs that come and go while running (monitor powered off and back on,
// unplugged, hotplugged). Kept free of Wayland types so it can be unit tested without a compositor.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "WallpaperEngine/Application/ApplicationContext.h"

namespace WallpaperEngine::Render::Drivers::Output::WaylandOutputSelection {
/**
 * @return Whether the user asked for a wallpaper on this output, with --screen-root or as part of a span group
 */
inline bool isOutputRequested (const std::string& name, const Application::ApplicationContext& context) {
    const auto& general = context.settings.general;

    if (general.screenBackgrounds.contains (name)) {
	return true;
    }

    return std::ranges::any_of (general.spanGroups, [&name] (const auto& spanGroup) {
	return std::ranges::find (spanGroup.screens, name) != spanGroup.screens.end ();
    });
}

/**
 * @return The output bound to this wl_registry name, or screens.end ()
 */
template <typename Screen> auto findByWaylandName (std::vector<Screen*>& screens, uint32_t waylandName) {
    return std::ranges::find_if (screens, [waylandName] (const Screen* screen) {
	return screen->waylandName == waylandName;
    });
}

/**
 * Outputs announced after startup (or whose layer surface was closed) whose wl_output.done has arrived, so
 * their name is known and their layer surface can be set up. A copy, since setting one up can add others.
 */
template <typename Screen> std::vector<Screen*> outputsReadyForSetup (const std::vector<Screen*>& screens) {
    std::vector<Screen*> ready;

    for (Screen* screen : screens) {
	if (screen->pendingSetup && screen->initialized && !screen->removed) {
	    ready.push_back (screen);
	}
    }

    return ready;
}

/** How many times an output that lost its layer surface is set up again right away */
constexpr int IMMEDIATE_SETUP_RETRIES = 3;

/**
 * An output lost its layer surface while staying present (the compositor closed it, or setting it up failed).
 * Set it up again on the next event loop pass, a few times; after that wait for its next wl_output.done, so a
 * compositor that keeps closing the surface can't make us loop.
 */
template <typename Screen> void scheduleSetupRetry (Screen& screen) {
    screen.pendingSetup = true;

    if (++screen.setupRetries > IMMEDIATE_SETUP_RETRIES) {
	screen.initialized = false;
    }
}

/**
 * wl_output.done: the output's properties are complete (its name is known), and it may have been reconfigured,
 * so allow immediate retries again
 */
template <typename Screen> void outputDone (Screen& screen) {
    screen.initialized = true;
    screen.setupRetries = 0;
}

/**
 * Takes the outputs the compositor removed out of the list. Removal only marks them, so a viewport that is
 * being set up or rendered when its removal arrives (setupLS does roundtrips) stays valid until the event
 * loop frees it.
 *
 * @return The removed outputs, for the caller to free
 */
template <typename Screen> std::vector<Screen*> takeRemoved (std::vector<Screen*>& screens) {
    std::vector<Screen*> removed;

    std::erase_if (screens, [&removed] (Screen* screen) {
	if (!screen->removed) {
	    return false;
	}

	removed.push_back (screen);
	return true;
    });

    return removed;
}
} // namespace WallpaperEngine::Render::Drivers::Output::WaylandOutputSelection
