#include "common.h"
#include "CWaylandOutput.h"
#include "../CWaylandOpenGLDriver.h"

using namespace WallpaperEngine::Render::Drivers::Output;

CWaylandOutput::CWaylandOutput (CApplicationContext& context, CVideoDriver& driver, Detectors::CFullScreenDetector& detector) :
    COutput (context, detector),
    m_driver (driver)
{
    updateViewports();
}

CWaylandOutput::~CWaylandOutput ()
{
}

void CWaylandOutput::updateViewports() {
    m_viewports.clear();
    const auto PDRIVER = (CWaylandOpenGLDriver*)&m_driver;
    glm::ivec2 fullw = {0,0};
    for (auto& o : PDRIVER->m_outputs) {
        m_viewports[o->name] = {{0, 0, o->lsSize.x * o->scale, o->lsSize.y * o->scale}, o->name};

        fullw = fullw + glm::ivec2{o->lsSize.x * o->scale, 0};
    }

    m_fullWidth = fullw.x;
    m_fullHeight = fullw.y;
}

void CWaylandOutput::reset () {
    updateViewports();
}

bool CWaylandOutput::renderVFlip () const {
    return true;
}

bool CWaylandOutput::renderMultiple () const  {
    return false; // todo
}

bool CWaylandOutput::haveImageBuffer () const {
    return false;
}

void* CWaylandOutput::getImageBuffer () const {
    return nullptr;
}

void CWaylandOutput::updateRender () const {
    ;
}