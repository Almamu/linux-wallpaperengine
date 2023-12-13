#include "COutput.h"

using namespace WallpaperEngine::Render::Drivers::Output;

COutput::COutput (CApplicationContext& context, CVideoDriver& driver) :
    m_fullWidth (0),
    m_fullHeight (0),
    m_context (context),
    m_driver (driver) {}

const std::map<std::string, COutputViewport*>& COutput::getViewports () const {
    return this->m_viewports;
}

int COutput::getFullWidth () const {
    return this->m_fullWidth;
}

int COutput::getFullHeight () const {
    return this->m_fullHeight;
}
