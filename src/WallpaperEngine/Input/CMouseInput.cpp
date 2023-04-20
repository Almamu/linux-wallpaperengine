#include <glm/common.hpp>
#include "CMouseInput.h"

using namespace WallpaperEngine::Input;

CMouseInput::CMouseInput (GLFWwindow* window) : position (), m_mousePosition (), m_window (window) {}

CMouseInput::CMouseInput(WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver) {
    waylandDriver = driver;
}

void CMouseInput::update ()
{
    if (!m_window) {
        if (!waylandDriver || !waylandDriver->lastLSInFocus)
            return;

        this->position = waylandDriver->lastLSInFocus->mousePos;

        return;
    }

    // update current mouse position
    glfwGetCursorPos (this->m_window, &this->m_mousePosition.x, &this->m_mousePosition.y);
    // interpolate to the new position
    this->position = glm::mix (this->position, this->m_mousePosition, 1.0);
}