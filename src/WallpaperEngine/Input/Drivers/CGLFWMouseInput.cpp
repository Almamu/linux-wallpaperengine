#include "CGLFWMouseInput.h"
#include <glm/common.hpp>

#include "WallpaperEngine/Render/Drivers/CGLFWOpenGLDriver.h"

using namespace WallpaperEngine::Input::Drivers;

CGLFWMouseInput::CGLFWMouseInput (Render::Drivers::CGLFWOpenGLDriver* driver) :
    m_driver (driver),
    m_mousePosition (),
    m_reportedPosition (),
    m_leftClick (MouseClickStatus::Released),
    m_rightClick (MouseClickStatus::Released) {}

void CGLFWMouseInput::update () {
    if (!this->m_driver->getApp ().getContext ().settings.mouse.enabled) {
        this->m_reportedPosition = {0, 0};
        return;
    }

    int leftClickState = glfwGetMouseButton (this->m_driver->getWindow (), GLFW_MOUSE_BUTTON_LEFT);
    int rightClickState = glfwGetMouseButton (this->m_driver->getWindow (), GLFW_MOUSE_BUTTON_RIGHT);

    this->m_leftClick = leftClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;
    this->m_rightClick = rightClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;

    // update current mouse position
    glfwGetCursorPos (this->m_driver->getWindow (), &this->m_mousePosition.x, &this->m_mousePosition.y);
    // interpolate to the new position
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, this->m_mousePosition, 1.0);
}

glm::dvec2 CGLFWMouseInput::position () const {
    return this->m_reportedPosition;
}

WallpaperEngine::Input::MouseClickStatus CGLFWMouseInput::leftClick () const {
    return m_leftClick;
}

WallpaperEngine::Input::MouseClickStatus CGLFWMouseInput::rightClick () const {
    return m_rightClick;
}