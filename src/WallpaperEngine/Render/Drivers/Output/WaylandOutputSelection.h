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
 * Outputs announced after startup whose wl_output.done has arrived, so their name is known and their layer
 * surface can be set up. Returned by wl_registry name, since setting one up can add or remove others.
 */
template <typename Screen> std::vector<uint32_t> outputsReadyForSetup (const std::vector<Screen*>& screens) {
    std::vector<uint32_t> ready;

    for (const Screen* screen : screens) {
	if (screen->pendingSetup && screen->initialized) {
	    ready.push_back (screen->waylandName);
	}
    }

    return ready;
}
} // namespace WallpaperEngine::Render::Drivers::Output::WaylandOutputSelection
