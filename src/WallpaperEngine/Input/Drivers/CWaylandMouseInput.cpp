#include "CWaylandMouseInput.h"
#include <glm/common.hpp>

using namespace WallpaperEngine::Input::Drivers;

CWaylandMouseInput::CWaylandMouseInput (WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver) :
    waylandDriver (driver),
    pos () {}

void CWaylandMouseInput::update () {}

glm::dvec2 CWaylandMouseInput::position () const {
    if (!this->waylandDriver->getApp ().getContext ().settings.mouse.enabled) {
        return {0, 0};
    }

    if (waylandDriver->viewportInFocus && waylandDriver->viewportInFocus->rendering)
        return waylandDriver->viewportInFocus->mousePos;

    return {0, 0};
}