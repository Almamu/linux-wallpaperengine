#include "CWaylandMouseInput.h"
#include <glm/common.hpp>

using namespace WallpaperEngine::Input::Drivers;

CWaylandMouseInput::CWaylandMouseInput (WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver) :
    waylandDriver (driver),
    m_pos () {}

void CWaylandMouseInput::update () {}

glm::dvec2 CWaylandMouseInput::position () const {
    if (!this->waylandDriver->getApp ().getContext ().settings.mouse.enabled) {
        return {0, 0};
    }

    if (waylandDriver->viewportInFocus && waylandDriver->viewportInFocus->rendering)
        return waylandDriver->viewportInFocus->mousePos;

    return {0, 0};
}

WallpaperEngine::Input::MouseClickStatus CWaylandMouseInput::leftClick () const {
    if (waylandDriver->viewportInFocus && waylandDriver->viewportInFocus->rendering)
        return waylandDriver->viewportInFocus->leftClick;

    return MouseClickStatus::Released;
}

WallpaperEngine::Input::MouseClickStatus CWaylandMouseInput::rightClick () const {
    if (waylandDriver->viewportInFocus && waylandDriver->viewportInFocus->rendering)
        return waylandDriver->viewportInFocus->rightClick;

    return MouseClickStatus::Released;
}