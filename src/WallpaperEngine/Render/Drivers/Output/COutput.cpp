#include "COutput.h"

using namespace WallpaperEngine::Render::Drivers::Output;

COutput::COutput (CApplicationContext& context, Detectors::CFullScreenDetector& detector) :
    m_context (context),
    m_detector (detector)
{
}

const std::map <std::string, COutput::ScreenInfo>& COutput::getViewports () const
{
    return this->m_viewports;
}

int COutput::getFullWidth () const
{
    return this->m_fullWidth;
}

int COutput::getFullHeight () const
{
    return this->m_fullHeight;
}
