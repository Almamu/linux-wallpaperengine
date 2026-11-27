#include "GLFWMouseInput.h"
#include <glm/common.hpp>

#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"

using namespace WallpaperEngine::Input::Drivers;

GLFWMouseInput::GLFWMouseInput (const Render::Drivers::GLFWOpenGLDriver& driver) : m_driver (driver) { }

void GLFWMouseInput::update () {
    if (!this->m_driver.getApp ().getContext ().settings.mouse.enabled) {
	this->m_reportedPosition = { 0, 0 };
	this->m_cursorPosition = { 0, 0 };
	return;
    }

    const int leftClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_LEFT);
    const int rightClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_RIGHT);

    this->m_leftClick = leftClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;
    this->m_rightClick = rightClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;

    glfwGetCursorPos (this->m_driver.getWindow (), &this->m_cursorPosition.x, &this->m_cursorPosition.y);

    // Preserve the existing bottom-left position used by the scene renderer.
    const glm::dvec2 position { this->m_cursorPosition.x,
        this->m_driver.getFramebufferSize ().y - this->m_cursorPosition.y };
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, position, 1.0);
}

glm::dvec2 GLFWMouseInput::position () const { return this->m_reportedPosition; }

glm::dvec2 GLFWMouseInput::screenPosition () const {
    // GLFW cursor coordinates are relative to the window's logical content area.
    glm::ivec2 windowSize;
    glfwGetWindowSize (this->m_driver.getWindow (), &windowSize.x, &windowSize.y);
    const auto framebufferSize = this->m_driver.getFramebufferSize ();
    return this->m_cursorPosition * glm::dvec2 (framebufferSize) / glm::dvec2 (glm::max (windowSize, glm::ivec2 (1)));
}

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::leftClick () const { return m_leftClick; }

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::rightClick () const { return m_rightClick; }