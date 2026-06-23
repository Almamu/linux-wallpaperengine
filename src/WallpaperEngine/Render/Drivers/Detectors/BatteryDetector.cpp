#include "BatteryDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <fstream>
#include <string>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Drivers::Detectors;

BatteryDetector::BatteryDetector (Application::ApplicationContext& appContext) : m_applicationContext (appContext) {}

Application::ApplicationContext& BatteryDetector::getApplicationContext () const { return this->m_applicationContext; }

bool BatteryDetector::isOnBattery () {
    const auto now = std::chrono::steady_clock::now ();

    // throttle the sysfs read: reuse the cached value if less than 5 seconds have passed
    if (std::chrono::duration_cast<std::chrono::seconds> (now - this->m_lastCheckTime).count () < 5) {
	return this->m_lastBatteryStatus;
    }

    this->m_lastCheckTime = now;

    // different platforms/kernels expose the AC adapter under different names
    std::ifstream file ("/sys/class/power_supply/AC/online");
    if (!file.is_open ()) {
	file.open ("/sys/class/power_supply/ACAD/online");
    }
    if (!file.is_open ()) {
	file.open ("/sys/class/power_supply/ADP1/online");
    }

    if (file.is_open ()) {
	std::string status;
	std::getline (file, status);
	// "0" means the AC adapter is offline, so we are running on battery
	this->m_lastBatteryStatus = (status == "0");

	if (this->m_lastBatteryStatus != this->m_lastLoggedStatus) {
	    sLog.out ("[BATTERY] Power source changed to: ", this->m_lastBatteryStatus ? "Battery" : "AC");
	    this->m_lastLoggedStatus = this->m_lastBatteryStatus;
	}

	return this->m_lastBatteryStatus;
    }

    // if no AC adapter file exists we cannot tell, so assume we are on AC and keep rendering
    sLog.error ("[BATTERY] Power source file not found, assuming AC power");
    this->m_lastBatteryStatus = false;
    return this->m_lastBatteryStatus;
}
