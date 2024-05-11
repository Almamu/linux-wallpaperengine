#pragma once

#include "WallpaperEngine/Input/CMouseInput.h"

#include <glm/vec2.hpp>

namespace WallpaperEngine::Render::Drivers {
class CGLFWOpenGLDriver;
}

namespace WallpaperEngine::Input::Drivers {
/**
 * Handles mouse input for the background
 */
class CGLFWMouseInput final : public CMouseInput {
  public:
    explicit CGLFWMouseInput (Render::Drivers::CGLFWOpenGLDriver* driver);

    /**
     * Takes current mouse position and updates it
     */
    void update () override;

    /**
     * The virtual pointer's position
     */
    [[nodiscard]] glm::dvec2 position () const override;

  private:
    Render::Drivers::CGLFWOpenGLDriver* m_driver;

    /**
     * The current mouse position
     */
    glm::dvec2 m_mousePosition;
    glm::dvec2 m_reportedPosition;
};
} // namespace WallpaperEngine::Input::Drivers
