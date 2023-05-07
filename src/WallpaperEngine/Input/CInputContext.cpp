#include "CInputContext.h"

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

CInputContext::CInputContext (CMouseInput* mouseInput) :
    m_mouse (mouseInput)
{
}

void CInputContext::update ()
{
    this->m_mouse->update ();
}

const CMouseInput& CInputContext::getMouseInput () const
{
    return *this->m_mouse;
}