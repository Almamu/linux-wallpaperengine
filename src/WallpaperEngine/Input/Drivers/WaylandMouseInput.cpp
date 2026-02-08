#include "WaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/WaylandOpenGLDriver.h"
#include <glm/common.hpp>

using namespace WallpaperEngine::Input::Drivers;

WaylandMouseInput::WaylandMouseInput (const WallpaperEngine::Render::Drivers::WaylandOpenGLDriver& driver) :
    m_waylandDriver (driver) { }

void WaylandMouseInput::update () { }

glm::dvec2 WaylandMouseInput::position () const {
    if (!this->m_waylandDriver.getApp ().getContext ().settings.mouse.enabled) {
	return { 0, 0 };
    }

    if (m_waylandDriver.viewportInFocus && m_waylandDriver.viewportInFocus->rendering) {
	return m_waylandDriver.viewportInFocus->mousePos;
    }

    return { 0, 0 };
}

WallpaperEngine::Input::MouseClickStatus WaylandMouseInput::leftClick () const {
    if (m_waylandDriver.viewportInFocus && m_waylandDriver.viewportInFocus->rendering) {
	return m_waylandDriver.viewportInFocus->leftClick;
    }

    return MouseClickStatus::Released;
}

WallpaperEngine::Input::MouseClickStatus WaylandMouseInput::rightClick () const {
    if (m_waylandDriver.viewportInFocus && m_waylandDriver.viewportInFocus->rendering) {
	return m_waylandDriver.viewportInFocus->rightClick;
    }

    return MouseClickStatus::Released;
}