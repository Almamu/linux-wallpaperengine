#include <glm/common.hpp>
#include "CWaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"

using namespace WallpaperEngine::Input::Drivers;

CWaylandMouseInput::CWaylandMouseInput (const WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver& driver) :
    m_waylandDriver (driver) {}

void CWaylandMouseInput::update () {}

glm::dvec2 CWaylandMouseInput::position () const {
    if (!this->m_waylandDriver.getApp ().getContext ().settings.mouse.enabled) {
        return {0, 0};
    }

    if (m_waylandDriver.viewportInFocus && m_waylandDriver.viewportInFocus->rendering)
        return m_waylandDriver.viewportInFocus->mousePos;

    return {0, 0};
}

WallpaperEngine::Input::MouseClickStatus CWaylandMouseInput::leftClick () const {
    if (m_waylandDriver.viewportInFocus && m_waylandDriver.viewportInFocus->rendering)
        return m_waylandDriver.viewportInFocus->leftClick;

    return MouseClickStatus::Released;
}

WallpaperEngine::Input::MouseClickStatus CWaylandMouseInput::rightClick () const {
    if (m_waylandDriver.viewportInFocus && m_waylandDriver.viewportInFocus->rendering)
        return m_waylandDriver.viewportInFocus->rightClick;

    return MouseClickStatus::Released;
}