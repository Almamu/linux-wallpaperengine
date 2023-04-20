#include <glm/common.hpp>
#include "CWaylandMouseInput.h"

using namespace WallpaperEngine::Input;

CWaylandMouseInput::CWaylandMouseInput(WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver) {
    waylandDriver = driver;
}

void CWaylandMouseInput::update ()
{
    ;
}

glm::dvec2 CWaylandMouseInput::position() const {
    if (!waylandDriver || !waylandDriver->lastLSInFocus)
        return {0, 0};

    return waylandDriver->lastLSInFocus->mousePos;
}