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

    for (auto& o : waylandDriver->m_outputs) {
        if (!o->rendering)
            continue;
        
        return o->layerSurface.get() == waylandDriver->lastLSInFocus ? o->layerSurface->mousePos : glm::dvec2{-1337, -1337};
    }

    return {0, 0};
}