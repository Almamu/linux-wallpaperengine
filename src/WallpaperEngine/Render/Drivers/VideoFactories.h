#pragma once

#include <functional>
#include <memory>
#include <map>

#include "VideoDriver.h"

#define DEFAULT_WINDOW_NAME "default"

namespace WallpaperEngine::Render::Drivers {
class VideoFactories {
  public:
    using DriverConstructionFunc = std::function<std::unique_ptr<VideoDriver>(ApplicationContext&, WallpaperApplication&)>;
    using FullscreenDetectorConstructionFunc = std::function<std::unique_ptr<Detectors::FullScreenDetector>(ApplicationContext&, VideoDriver&)>;
    VideoFactories ();

    static VideoFactories& get ();

    /**
     * Adds a new handler for the given window mode and XDG_SESSION_TYPE
     *
     * @param forMode
     * @param xdgSessionType
     * @param factory
     */
    void registerDriver (
        ApplicationContext::WINDOW_MODE forMode, std::string xdgSessionType, DriverConstructionFunc factory);

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
     * @param context
     * @param application
     * @return
     */
    [[nodiscard]] std::unique_ptr <VideoDriver> createVideoDriver (
        ApplicationContext::WINDOW_MODE mode, const std::string& xdgSessionType,
        ApplicationContext& context, WallpaperApplication& application);

    /**
     * Calls the factory and builds the requested fullscreen detector or provides a stub if not possible
     *
     * @return
     */
    [[nodiscard]] std::unique_ptr <Detectors::FullScreenDetector> createFullscreenDetector (
        const std::string& xdgSessionType, ApplicationContext& context, VideoDriver& driver);

  private:
    using SessionTypeToFullscreenDetectorType = std::map <std::string, FullscreenDetectorConstructionFunc>;
    using SessionTypeToFactoryType = std::map <std::string, DriverConstructionFunc>;
    using WindowModeToSessionType = std::map <ApplicationContext::WINDOW_MODE, SessionTypeToFactoryType>;

    SessionTypeToFullscreenDetectorType m_fullscreenFactories = {};
    WindowModeToSessionType m_driverFactories = {};
    static std::unique_ptr<VideoFactories> sInstance;
};
} // namespace WallpaperEngine::Render::Drivers

#define sVideoFactories (WallpaperEngine::Render::Drivers::VideoFactories::get ())