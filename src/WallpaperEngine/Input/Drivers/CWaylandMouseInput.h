#pragma once

#ifdef ENABLE_WAYLAND

#include "WallpaperEngine/Input/CMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"

#include "GLFW/glfw3.h"
#include <glm/vec2.hpp>

namespace WallpaperEngine::Input::Drivers {
/**
 * Handles mouse input for the background
 */
class CWaylandMouseInput final : public CMouseInput {
  public:
    explicit CWaylandMouseInput (WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver);

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
    WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* waylandDriver = nullptr;

    glm::dvec2 m_pos;
};
} // namespace WallpaperEngine::Input::Drivers

#endif /* ENABLE_WAYLAND */