#pragma once

#ifdef ENABLE_WAYLAND

#include "WallpaperEngine/Input/CMouseInput.h"

#include <glm/vec2.hpp>

namespace WallpaperEngine::Render::Drivers {
class CWaylandOpenGLDriver;
};

namespace WallpaperEngine::Input::Drivers {
/**
 * Handles mouse input for the background
 */
class CWaylandMouseInput final : public CMouseInput {
  public:
    explicit CWaylandMouseInput (const WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver& driver);

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
    const WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver& m_waylandDriver;

    glm::dvec2 m_pos = {};
};
} // namespace WallpaperEngine::Input::Drivers

#endif /* ENABLE_WAYLAND */