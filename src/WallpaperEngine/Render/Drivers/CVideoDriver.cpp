#include "CVideoDriver.h"

using namespace WallpaperEngine::Render::Drivers;

void CVideoDriver::dispatchEventQueue() const {
    // intentionally left blank
}

void CVideoDriver::makeCurrent(const std::string& outputName) const {
    // intentionally left blank
}

bool CVideoDriver::shouldRenderOutput(const std::string& outputName) const {
    return true;
}

bool CVideoDriver::requiresSeparateFlips() const {
    return false;
}

void CVideoDriver::swapOutputBuffer(const std::string& outputName) {
    // intentionally left blank
}