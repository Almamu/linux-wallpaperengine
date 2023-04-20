#include "CInputContext.h"
#include "WallpaperEngine/Render/Drivers/CX11OpenGLDriver.h"
#include "WallpaperEngine/Input/CGLFWMouseInput.h"
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"
#include "WallpaperEngine/Input/CWaylandMouseInput.h"
#endif

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

CInputContext::CInputContext (CX11OpenGLDriver& videoDriver)
{
    m_mouse = std::make_unique<CGLFWMouseInput>(videoDriver.getWindow());
}

#ifdef ENABLE_WAYLAND
CInputContext::CInputContext (CWaylandOpenGLDriver& videoDriver)
{
    m_mouse = std::make_unique<CWaylandMouseInput>(&videoDriver);
}
#endif

void CInputContext::update ()
{
    this->m_mouse->update ();
}

const CMouseInput& CInputContext::getMouseInput () const
{
    return *this->m_mouse.get();
}