#include "CWaylandMouseInput.h"
#include <glm/common.hpp>

using namespace WallpaperEngine::Input::Drivers;

CWaylandMouseInput::CWaylandMouseInput (WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver) :
    waylandDriver (driver),
    m_pos () {}

void CWaylandMouseInput::update () {
    if (!this->waylandDriver->getApp ().getContext ().settings.mouse.enabled) {
        return;
    }

    if (!waylandDriver->viewportInFocus || !waylandDriver->viewportInFocus->rendering) {
        return;
    }

    // TODO: IS CLEARING STATE HERE A GOOD SOLUTION? OR SHOULD BE HANDLED SOMEWHERE ELSE?
    if (waylandDriver->viewportInFocus->leftClick == MouseClickStatus::Released) {
        waylandDriver->viewportInFocus->leftClick = MouseClickStatus::Waiting;
    }

    if (waylandDriver->viewportInFocus->rightClick == MouseClickStatus::Released) {
        waylandDriver->viewportInFocus->rightClick = MouseClickStatus::Waiting;
    }
}

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

    return MouseClickStatus::Waiting;
}

WallpaperEngine::Input::MouseClickStatus CWaylandMouseInput::rightClick () const {
    if (waylandDriver->viewportInFocus && waylandDriver->viewportInFocus->rendering)
        return waylandDriver->viewportInFocus->rightClick;

    return MouseClickStatus::Waiting;
}