#include "CWaylandOutput.h"
#include "../CWaylandOpenGLDriver.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "common.h"

using namespace WallpaperEngine::Render::Drivers::Output;

CWaylandOutput::CWaylandOutput (CApplicationContext& context, CWaylandOpenGLDriver& driver) :
    COutput (context, driver) {
    updateViewports ();
}

void CWaylandOutput::updateViewports () {
    m_viewports.clear ();
    const auto PDRIVER = dynamic_cast<CWaylandOpenGLDriver*> (&m_driver);
    glm::ivec2 fullw = {0, 0};
    for (const auto& o : PDRIVER->m_screens) {
        if (!o->layerSurface)
            continue;

        m_viewports [o->name] = o;

        fullw = fullw + glm::ivec2 {o->size.x * o->scale, 0};
        if (o->size.y > fullw.y)
            fullw.y = o->size.y;
    }

    m_fullWidth = fullw.x;
    m_fullHeight = fullw.y;
}

void CWaylandOutput::reset () {
    updateViewports ();
}

bool CWaylandOutput::renderVFlip () const {
    return true;
}

bool CWaylandOutput::renderMultiple () const {
    return false; // todo
}

bool CWaylandOutput::haveImageBuffer () const {
    return false;
}

void* CWaylandOutput::getImageBuffer () const {
    return nullptr;
}

uint32_t CWaylandOutput::getImageBufferSize () const {
    return 0;
}

void CWaylandOutput::updateRender () const {}