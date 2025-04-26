#include "CVideoDriver.h"

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

CVideoDriver::CVideoDriver (CWallpaperApplication& app, CMouseInput& mouseInput) :
    m_app (app),
    m_inputContext (mouseInput) {}

CWallpaperApplication& CVideoDriver::getApp () const {
    return this->m_app;
}

CInputContext& CVideoDriver::getInputContext () {
    return this->m_inputContext;
}