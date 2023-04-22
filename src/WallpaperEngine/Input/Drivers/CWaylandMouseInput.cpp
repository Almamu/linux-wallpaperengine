#include <glm/common.hpp>
#include "CWaylandMouseInput.h"

using namespace WallpaperEngine::Input::Drivers;

CWaylandMouseInput::CWaylandMouseInput(WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver) :
    waylandDriver (driver)
{
}

void CWaylandMouseInput::update ()
{
}

glm::dvec2 CWaylandMouseInput::position() const
{
    if (!waylandDriver->viewportInFocus)
        return {0, 0};

    for (auto& o : waylandDriver->m_screens)
    {
        if (!o->rendering)
            continue;

        return o == waylandDriver->viewportInFocus ? o->mousePos : glm::dvec2{-1337, -1337};
    }

    return {0, 0};
}