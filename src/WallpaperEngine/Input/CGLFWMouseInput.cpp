#include <glm/common.hpp>
#include "CGLFWMouseInput.h"

using namespace WallpaperEngine::Input;

CGLFWMouseInput::CGLFWMouseInput (GLFWwindow* window) : m_reportedPosition (), m_mousePosition (), m_window (window) {}

void CGLFWMouseInput::update ()
{
    if (!m_window)
        return;

    // update current mouse position
    glfwGetCursorPos (this->m_window, &this->m_mousePosition.x, &this->m_mousePosition.y);
    // interpolate to the new position
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, this->m_mousePosition, 1.0);
}

glm::dvec2 CGLFWMouseInput::position() const {
    return this->m_reportedPosition;
}