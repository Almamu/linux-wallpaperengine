#include "CGLFWMouseInput.h"
#include <glm/common.hpp>

#include "WallpaperEngine/Render/Drivers/CGLFWOpenGLDriver.h"

using namespace WallpaperEngine::Input::Drivers;

CGLFWMouseInput::CGLFWMouseInput (Render::Drivers::CGLFWOpenGLDriver* driver) :
    m_reportedPosition (),
    m_mousePosition (),
    m_driver (driver) {}

void CGLFWMouseInput::update () {
    if (!this->m_driver->getApp ().getContext ().settings.mouse.enabled) {
        this->m_reportedPosition = {0, 0};
        return;
    }

    // update current mouse position
    glfwGetCursorPos (this->m_driver->getWindow (), &this->m_mousePosition.x, &this->m_mousePosition.y);
    // interpolate to the new position
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, this->m_mousePosition, 1.0);
}

glm::dvec2 CGLFWMouseInput::position () const {
    return this->m_reportedPosition;
}