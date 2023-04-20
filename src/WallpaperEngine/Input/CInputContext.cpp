#include "CInputContext.h"
#include "WallpaperEngine/Render/Drivers/CX11OpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

CInputContext::CInputContext (CX11OpenGLDriver& videoDriver) :
    m_mouse (videoDriver.getWindow ())
{
}

#ifdef ENABLE_WAYLAND
CInputContext::CInputContext (CWaylandOpenGLDriver& videoDriver) :
    m_mouse (nullptr)
{
    // todo
}
#endif

void CInputContext::update ()
{
    this->m_mouse.update ();
}

const CMouseInput& CInputContext::getMouseInput () const
{
    return this->m_mouse;
}