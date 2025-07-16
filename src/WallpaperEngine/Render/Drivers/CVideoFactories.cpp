#include <algorithm>

#include "CVideoFactories.h"
#include "WallpaperEngine/Logging/CLog.h"
#include <cassert>

using namespace WallpaperEngine::Render::Drivers;

CVideoFactories::CVideoFactories () {
    assert (this->sInstance == nullptr);
}

CVideoFactories& CVideoFactories::get () {
    if (sInstance == nullptr) {
        sInstance = std::make_unique <CVideoFactories> ();
    }

    return *sInstance;
}

void CVideoFactories::registerDriver (
    CApplicationContext::WINDOW_MODE forMode, std::string xdgSessionType,
    WallpaperEngine::Render::Drivers::CVideoFactories::DriverConstructionFunc factory) {
    const auto cur = this->m_driverFactories.find (forMode);

    if (cur == this->m_driverFactories.end ()) {
        SessionTypeToFactoryType map;

        map.emplace (xdgSessionType, factory);
        this->m_driverFactories.emplace (forMode, map);
    } else {
        cur->second.emplace (xdgSessionType, factory);
    }
}

void CVideoFactories::registerFullscreenDetector (std::string xdgSessionType, FullscreenDetectorConstructionFunc factory) {
    this->m_fullscreenFactories.emplace (xdgSessionType, factory);
}

std::vector<std::string> CVideoFactories::getRegisteredDrivers () const {
    std::vector<std::string> result;

    for (const auto& [windowMode, sessionTypeToFactory] : this->m_driverFactories) {
        for (const auto& [xdgSessionType, _] : sessionTypeToFactory) {
            result.push_back (xdgSessionType);
        }
    }

    auto last = std::unique (result.begin (), result.end ());

    result.erase (last, result.end ());

    return result;
}

std::unique_ptr <CVideoDriver> CVideoFactories::createVideoDriver (
    CApplicationContext::WINDOW_MODE mode, const std::string& xdgSessionType,
    CApplicationContext& context, CWallpaperApplication& application
) {
    const auto sessionTypeToFactory = this->m_driverFactories.find (mode);

    if (sessionTypeToFactory == this->m_driverFactories.end ()) {
        sLog.exception ("Cannot find a driver for window mode ", mode, " and XDG_SESSION_TYPE ", xdgSessionType);
    }

    // windows are a bit special, there's just one handler
    // and it's not like the current map properly allows for storing this
    // so hijacking the detection is probably best for now
    const auto factory =
        mode != Application::CApplicationContext::DESKTOP_BACKGROUND
            ? sessionTypeToFactory->second.find (DEFAULT_WINDOW_NAME)
            : sessionTypeToFactory->second.find (xdgSessionType);

    if (factory == sessionTypeToFactory->second.end ()) {
        sLog.exception ("Cannot find a driver for window mode ", mode, " and XDG_SESSION_TYPE ", xdgSessionType);
    }

    return factory->second (context, application);
}

std::unique_ptr <Detectors::CFullScreenDetector> CVideoFactories::createFullscreenDetector (
    std::string xdgSessionType, CApplicationContext& context, CVideoDriver& driver
) {
    const auto it = this->m_fullscreenFactories.find (xdgSessionType);

    if (it == this->m_fullscreenFactories.end () || !context.settings.render.pauseOnFullscreen) {
        return std::make_unique <Detectors::CFullScreenDetector> (context);
    }

    return it->second (context, driver);
}

std::unique_ptr<CVideoFactories> CVideoFactories::sInstance = nullptr;