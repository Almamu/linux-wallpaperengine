#include "BatteryDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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

    // the AC adapter name varies between vendors/kernels/drivers, so instead of
    // hardcoding names we enumerate every power supply and look at the ones reporting
    // type "Mains" (the standard recommended pattern on Linux)
    bool foundPowerSource = false;
    bool onBattery = true;

    std::error_code ec;
    const std::filesystem::directory_iterator end {};
    for (std::filesystem::directory_iterator it ("/sys/class/power_supply", ec); !ec && it != end; it.increment (ec)) {
	std::ifstream typeFile (it->path () / "type");
	if (!typeFile.is_open ()) {
	    continue;
	}

	std::string type;
	std::getline (typeFile, type);
	if (type != "Mains") {
	    continue;
	}

	std::ifstream onlineFile (it->path () / "online");
	if (!onlineFile.is_open ()) {
	    continue;
	}

	std::string online;
	std::getline (onlineFile, online);

	foundPowerSource = true;
	// "1" means this AC adapter is plugged in, so we are not on battery
	if (online == "1") {
	    onBattery = false;
	    break;
	}
    }

    if (foundPowerSource) {
	this->m_lastBatteryStatus = onBattery;

	if (this->m_lastBatteryStatus != this->m_lastLoggedStatus) {
	    sLog.out ("[BATTERY] Power source changed to: ", this->m_lastBatteryStatus ? "Battery" : "AC");
	    this->m_lastLoggedStatus = this->m_lastBatteryStatus;
	}

	return this->m_lastBatteryStatus;
    }

    // no AC adapter was found (e.g. a desktop without a "Mains" supply); we cannot tell,
    // so assume we are on AC and keep rendering. log this only once to avoid spamming the
    // output on every cache refresh
    if (!this->m_missingPowerSourceLogged) {
	sLog.out ("[BATTERY] No AC power source found in /sys/class/power_supply, assuming AC power");
	this->m_missingPowerSourceLogged = true;
    }

    this->m_lastBatteryStatus = false;
    return this->m_lastBatteryStatus;
}
