#include "VideoDriver.h"

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

VideoDriver::VideoDriver (WallpaperApplication& app, MouseInput& mouseInput) :
    m_app (app), m_inputContext (mouseInput) { }

wl_display* VideoDriver::getWaylandDisplay () const { return nullptr; }

WallpaperApplication& VideoDriver::getApp () const { return this->m_app; }

InputContext& VideoDriver::getInputContext () { return this->m_inputContext; }