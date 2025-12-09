#include "GLFWMouseInput.h"
#include <glm/common.hpp>

#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"

using namespace WallpaperEngine::Input::Drivers;

GLFWMouseInput::GLFWMouseInput (const Render::Drivers::GLFWOpenGLDriver& driver) :
    m_driver (driver) {}

void GLFWMouseInput::update () {
    if (!this->m_driver.getApp ().getContext ().settings.mouse.enabled) {
        this->m_reportedPosition = {0, 0};
        return;
    }

    const int leftClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_LEFT);
    const int rightClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_RIGHT);

    this->m_leftClick = leftClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;
    this->m_rightClick = rightClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;

    // update current mouse position
    glfwGetCursorPos (this->m_driver.getWindow (), &this->m_mousePosition.x, &this->m_mousePosition.y);
    
    // Convert from GLFW coordinate system (Y=0 at top) to OpenGL coordinate system (Y=0 at bottom)
    const glm::ivec2 framebufferSize = this->m_driver.getFramebufferSize ();
    this->m_mousePosition.y = static_cast<double> (framebufferSize.y) - this->m_mousePosition.y;
    
    // interpolate to the new position
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, this->m_mousePosition, 1.0);
}

glm::dvec2 GLFWMouseInput::position () const {
    return this->m_reportedPosition;
}

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::leftClick () const {
    return m_leftClick;
}

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::rightClick () const {
    return m_rightClick;
}