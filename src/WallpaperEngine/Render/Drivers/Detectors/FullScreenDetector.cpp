#include "FullScreenDetector.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Drivers::Detectors;

FullScreenDetector::FullScreenDetector (Application::ApplicationContext& appContext) :
    m_applicationContext (appContext) {}

Application::ApplicationContext& FullScreenDetector::getApplicationContext () const {
    return this->m_applicationContext;
}

bool FullScreenDetector::anythingFullscreen () const {
    return false;
}

void FullScreenDetector::reset () {}