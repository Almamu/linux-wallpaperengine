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

    int leftClickState = glfwGetMouseButton (this->m_driver->getWindow (), GLFW_MOUSE_BUTTON_LEFT);
    int rightClickState = glfwGetMouseButton (this->m_driver->getWindow (), GLFW_MOUSE_BUTTON_RIGHT);

    if (leftClickState == GLFW_RELEASE) {
        if (this->m_leftClick == MouseClickStatus::Released) {
            this->m_leftClick = MouseClickStatus::Waiting;
        }

        if (this->m_leftClick == MouseClickStatus::Clicked) {
            this->m_leftClick = MouseClickStatus::Released;
        }
    } else {
        this->m_leftClick = MouseClickStatus::Clicked;
    }

    if (rightClickState == GLFW_RELEASE) {
        if (this->m_rightClick == MouseClickStatus::Released) {
            this->m_rightClick = MouseClickStatus::Waiting;
        }

        if (this->m_rightClick == MouseClickStatus::Clicked) {
            this->m_rightClick = MouseClickStatus::Released;
        }
    } else {
        this->m_rightClick = MouseClickStatus::Clicked;
    }

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