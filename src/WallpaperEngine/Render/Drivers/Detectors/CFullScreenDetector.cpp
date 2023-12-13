#include "CFullScreenDetector.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Drivers::Detectors;

CFullScreenDetector::CFullScreenDetector (Application::CApplicationContext& appContext) :
    m_applicationContext (appContext) {}

Application::CApplicationContext& CFullScreenDetector::getApplicationContext () const {
    return this->m_applicationContext;
}