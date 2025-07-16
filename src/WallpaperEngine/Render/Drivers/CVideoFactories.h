#pragma once

#include <functional>
#include <memory>
#include <map>

#include "CVideoDriver.h"

#define DEFAULT_WINDOW_NAME "default"

namespace WallpaperEngine::Render::Drivers {
class CVideoFactories {
  public:
    using DriverConstructionFunc = std::function<std::unique_ptr<CVideoDriver>(CApplicationContext&, CWallpaperApplication&)>;
    using FullscreenDetectorConstructionFunc = std::function<std::unique_ptr<Detectors::CFullScreenDetector>(CApplicationContext&, CVideoDriver&)>;
    CVideoFactories ();

    static CVideoFactories& get ();

    /**
     * Adds a new handler for the given window mode and XDG_SESSION_TYPE
     *
     * @param forMode
     * @param factory
     */
    void registerDriver (
        CApplicationContext::WINDOW_MODE forMode, std::string xdgSessionType, DriverConstructionFunc factory);

    /**
     * Adds a new handler for the given XDG_SESSION_TYPE
     *
     * @param xdgSessionType
     * @param factory
     */
    void registerFullscreenDetector (std::string xdgSessionType, FullscreenDetectorConstructionFunc factory);

    /**
     * @return List of drivers supported over all the different window modes
     */
    [[nodiscard]] std::vector<std::string> getRegisteredDrivers () const;

    /**
     * Calls the factory and builds the requested video driver
     *
     * @param mode
     * @param xdgSessionType
     * @return
     */
    [[nodiscard]] std::unique_ptr <CVideoDriver> createVideoDriver (
        CApplicationContext::WINDOW_MODE mode, const std::string& xdgSessionType,
        CApplicationContext& context, CWallpaperApplication& application);

    /**
     * Calls the factory and builds the requested fullscreen detector or provides a stub if not possible
     *
     * @return
     */
    [[nodiscard]] std::unique_ptr <Detectors::CFullScreenDetector> createFullscreenDetector (
        std::string xdgSessionType, CApplicationContext& context, CVideoDriver& driver);

  private:
    using SessionTypeToFullscreenDetectorType = std::map <std::string, FullscreenDetectorConstructionFunc>;
    using SessionTypeToFactoryType = std::map <std::string, DriverConstructionFunc>;
    using WindowModeToSessionType = std::map <CApplicationContext::WINDOW_MODE, SessionTypeToFactoryType>;

    SessionTypeToFullscreenDetectorType m_fullscreenFactories = {};
    WindowModeToSessionType m_driverFactories = {};
    static std::unique_ptr<CVideoFactories> sInstance;
};
} // namespace WallpaperEngine::Render::Drivers

#define sVideoFactories (WallpaperEngine::Render::Drivers::CVideoFactories::get ())