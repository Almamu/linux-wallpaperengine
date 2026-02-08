#pragma once

#include <glm/vec4.hpp>
#include <string>

namespace WallpaperEngine::Render::Drivers::Output {
class OutputViewport {
public:
    OutputViewport (glm::ivec4 viewport, std::string name, bool single = false);
    virtual ~OutputViewport () = default;

    glm::ivec4 viewport;
    std::string name;

    /** Whether this viewport is single in the framebuffer or shares space with more viewports */
    bool single;

    /**
     * Activates output's context for drawing
     */
    virtual void makeCurrent () = 0;

    /**
     * Swaps buffers to present data on the viewport
     */
    virtual void swapOutput () = 0;
};
} // namespace WallpaperEngine::Render::Drivers::Output
