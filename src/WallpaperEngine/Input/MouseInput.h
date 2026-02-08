#pragma once

#include <glm/vec2.hpp>

namespace WallpaperEngine::Input {
enum MouseClickStatus : int { Released = 0, Clicked = 1 };

/**
 * Handles mouse input for the background
 *
 * Coordinate System Convention:
 * - Engine uses OpenGL bottom-left origin (Y=0 at bottom, Y=height at top)
 * - Window systems (GLFW/Wayland) use top-left origin (Y=0 at top, Y=height at bottom)
 * - CEF/web uses top-left origin (Y=0 at top, Y=height at bottom)
 *
 * Input drivers convert from window system coordinates to OpenGL coordinates.
 * Render code normalizes OpenGL coordinates (0=bottom, 1=top).
 * Web wallpapers convert from OpenGL coordinates to CEF coordinates.
 */
class MouseInput {
public:
    virtual ~MouseInput () = default;
    /**
     * Takes current mouse position and updates it
     */
    virtual void update () = 0;

    /**
     * The virtual pointer's position
     */
    [[nodiscard]] virtual glm::dvec2 position () const = 0;

    /**
     * @return The status of the mouse's left click
     */
    [[nodiscard]] virtual MouseClickStatus leftClick () const = 0;

    /**
     * @return The status of the mouse's right click
     */
    [[nodiscard]] virtual MouseClickStatus rightClick () const = 0;
};
} // namespace WallpaperEngine::Input
