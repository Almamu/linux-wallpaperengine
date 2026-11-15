#pragma once

#include "OutputViewport.h"

namespace WallpaperEngine::Render::Drivers::Output {
/**
 * Viewport for an embedded host. The host's context is already current when the
 * renderer runs and the host presents the framebuffer itself, so both hooks are
 * intentionally empty.
 */
class EmbeddedOutputViewport final : public OutputViewport {
public:
    EmbeddedOutputViewport (glm::ivec4 viewport, std::string name);

    void makeCurrent () override;
    void swapOutput () override;
};
} // namespace WallpaperEngine::Render::Drivers::Output
