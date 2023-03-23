#include "COutput.h"

using namespace WallpaperEngine::Render::Drivers::Output;

COutput::COutput (CApplicationContext& context) :
    m_context (context)
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
