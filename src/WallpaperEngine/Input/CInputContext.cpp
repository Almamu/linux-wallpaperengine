#include "CInputContext.h"
#include "WallpaperEngine/Render/Drivers/CX11OpenGLDriver.h"

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

CInputContext::CInputContext (CX11OpenGLDriver& videoDriver) :
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