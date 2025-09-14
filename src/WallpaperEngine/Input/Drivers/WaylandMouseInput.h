#pragma once

#ifdef ENABLE_WAYLAND

#include "WallpaperEngine/Input/MouseInput.h"

#include <glm/vec2.hpp>

namespace WallpaperEngine::Render::Drivers {
class WaylandOpenGLDriver;
};

namespace WallpaperEngine::Input::Drivers {
/**
 * Handles mouse input for the background
 */
class WaylandMouseInput final : public MouseInput {
  public:
    explicit WaylandMouseInput (const WallpaperEngine::Render::Drivers::WaylandOpenGLDriver& driver);

    /**
     * Takes current mouse position and updates it
     */
    void update () override;

    /**
     * The virtual pointer's position
     */
    [[nodiscard]] glm::dvec2 position () const override;

    /**
     * @return The status of the mouse's left click
     */
    [[nodiscard]] MouseClickStatus leftClick () const override;

    /**
     * @return The status of the mouse's right click
     */
    [[nodiscard]] MouseClickStatus rightClick () const override;

  private:
    /**
     * Wayland: Driver
     */
    const WallpaperEngine::Render::Drivers::WaylandOpenGLDriver& m_waylandDriver;

    glm::dvec2 m_pos = {};
};
} // namespace WallpaperEngine::Input::Drivers

#endif /* ENABLE_WAYLAND */