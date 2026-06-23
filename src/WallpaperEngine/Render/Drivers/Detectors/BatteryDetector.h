#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include <chrono>

namespace WallpaperEngine::Render::Drivers::Detectors {
class BatteryDetector {
public:
    explicit BatteryDetector (Application::ApplicationContext& appContext);
    virtual ~BatteryDetector () = default;

    /**
     * @return true if the system is currently running on battery power
     */
    [[nodiscard]] virtual bool isOnBattery ();

    [[nodiscard]] Application::ApplicationContext& getApplicationContext () const;

private:
    Application::ApplicationContext& m_applicationContext;
    std::chrono::steady_clock::time_point m_lastCheckTime;
    bool m_lastBatteryStatus = false;
    bool m_lastLoggedStatus = false;
};
} // namespace WallpaperEngine::Render::Drivers::Detectors
