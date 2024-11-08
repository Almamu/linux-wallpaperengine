#pragma once

#include <glm/vec2.hpp>

namespace WallpaperEngine::Input {
enum MouseClickStatus : int {
    Waiting = 0,
    Released = 1,
    Clicked = 2
};

/**
 * Handles mouse input for the background
 */
class CMouseInput {
  public:
    virtual ~CMouseInput () = default;
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
