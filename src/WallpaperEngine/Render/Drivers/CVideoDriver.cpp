#include "CVideoDriver.h"

using namespace WallpaperEngine::Render::Drivers;

CVideoDriver::CVideoDriver (CWallpaperApplication& app) : m_app (app) {}

CWallpaperApplication& CVideoDriver::getApp () const {
    return this->m_app;
}