#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Assets/AssetLocator.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include "WallpaperEngine/Audio/Drivers/SDLAudioDriver.h"

#include "WallpaperEngine/Input/InputContext.h"
#include "WallpaperEngine/WebBrowser/WebBrowserContext.h"

#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Application {
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 */
class WallpaperApplication {
  public:
    explicit WallpaperApplication (ApplicationContext& context);

    /**
     * Shows the application until it's closed
     */
    void show ();
    /**
     * Handles a OS signal sent to this PID
     *
     * @param signal
     */
    void signal (int signal);
    /**
     * @return Maps screens to loaded backgrounds
     */
    [[nodiscard]] const std::map<std::string, ProjectUniquePtr>& getBackgrounds () const;
    /**
     * @return The current application context
     */
    [[nodiscard]] ApplicationContext& getContext () const;
    /**
     * Renders a frame
     */
    void update (Render::Drivers::Output::OutputViewport* viewport);
    /**
     * Gets the output
     */
    [[nodiscard]] const WallpaperEngine::Render::Drivers::Output::Output& getOutput () const;

  private:
    /**
     * Sets up an asset locator for the given background
     *
     * @param bg
     */
    AssetLocatorUniquePtr setupAssetLocator (const std::string& bg) const;
    /**
     * Loads projects based off the settings
     */
    void loadBackgrounds ();
    /**
     * Loads the given project
     *
     * @param bg
     * @return
     */
    [[nodiscard]] ProjectUniquePtr loadBackground (const std::string& bg);
    /**
     * Prepares all background's values and updates their properties if required
     */
    void setupProperties ();
    /**
     * Updates the properties for the given background based on the current context
     *
     * @param project
     */
    void setupPropertiesForProject (const Project& project);
    /**
     * Prepares CEF browser to be used
     */
    void setupBrowser ();
    /**
     * Prepares desktop environment-related things (like render, window, fullscreen detector, etc)
     */
    void setupOutput ();
    /**
     * Prepares all audio-related things (like detector, output, etc)
     */
    void setupAudio ();
    /**
     * Prepares the render-context of all the backgrounds so they can be displayed on the screen
     */
    void prepareOutputs ();
    /**
     * Takes an screenshot of the background and saves it to the specified path
     *
     * @param filename
     */
    void takeScreenshot (const std::filesystem::path& filename) const;

    /** The application context that contains the current app settings */
    ApplicationContext& m_context;
    /** Maps screens to backgrounds */
    std::map<std::string, ProjectUniquePtr> m_backgrounds {};

    std::unique_ptr <WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> m_audioDetector = nullptr;
    std::unique_ptr <WallpaperEngine::Audio::AudioContext> m_audioContext = nullptr;
    std::unique_ptr <WallpaperEngine::Audio::Drivers::SDLAudioDriver> m_audioDriver = nullptr;
    std::unique_ptr <WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> m_audioRecorder = nullptr;
    std::unique_ptr <WallpaperEngine::Render::RenderContext> m_renderContext = nullptr;
    std::unique_ptr <WallpaperEngine::Render::Drivers::VideoDriver> m_videoDriver = nullptr;
    std::unique_ptr <WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> m_fullScreenDetector = nullptr;
    std::unique_ptr <WallpaperEngine::WebBrowser::WebBrowserContext> m_browserContext = nullptr;
};
} // namespace WallpaperEngine::Application
