#include <glm/common.hpp>
#include "CGLFWMouseInput.h"

#include "WallpaperEngine/Render/Drivers/CX11OpenGLDriver.h"

using namespace WallpaperEngine::Input::Drivers;

CGLFWMouseInput::CGLFWMouseInput (Render::Drivers::CX11OpenGLDriver* driver) :
    m_reportedPosition (),
    m_mousePosition (),
    m_driver (driver) {}

void CGLFWMouseInput::update ()
{
    // update current mouse position
    glfwGetCursorPos (this->m_driver->getWindow (), &this->m_mousePosition.x, &this->m_mousePosition.y);
    // interpolate to the new position
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, this->m_mousePosition, 1.0);
}

glm::dvec2 CGLFWMouseInput::position() const
{
    return this->m_reportedPosition;
}