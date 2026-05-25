#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <string>

namespace WallpaperEngine::Render::Drivers::Output {
class OutputViewport {
public:
    OutputViewport (glm::ivec4 viewport, std::string name, bool single = false);
    virtual ~OutputViewport () = default;

    glm::ivec4 viewport;
    std::string name;
    /** Global position of this viewport in the combined desktop coordinate space */
    glm::ivec2 globalPosition = {0, 0};
    /** Logical (unscaled) size in the same coordinate space as globalPosition */
    glm::ivec2 logicalSize = {0, 0};

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
