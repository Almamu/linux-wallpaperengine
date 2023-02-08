#include "CInputContext.h"
#include "WallpaperEngine/Render/Drivers/COpenGLDriver.h"

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

CInputContext::CInputContext (COpenGLDriver& videoDriver) :
    m_mouse (videoDriver.getWindow ())
{
}

void CInputContext::update ()
{
    this->m_mouse.update ();
}

const CMouseInput& CInputContext::getMouseInput () const
{
    return this->m_mouse;
}