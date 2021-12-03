#include <glm/common.hpp>
#include "CMouseInput.h"

using namespace WallpaperEngine::Input;

CMouseInput::CMouseInput (GLFWwindow* window) : position(0, 0), m_window (window) {}

void CMouseInput::update ()
{
    // update current mouse position
    glfwGetCursorPos (this->m_window, &this->m_mousePosition.x, &this->m_mousePosition.y);
    // interpolate to the new position
    this->position = glm::mix (this->position, this->m_mousePosition, 1.0);
}