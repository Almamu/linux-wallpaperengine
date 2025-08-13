#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"

#include "WallpaperEngine/Assets/CCombinedContainer.h"

#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Drivers/CGLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/Output/CGLFWWindowOutput.h"

#include "WallpaperEngine/Audio/Drivers/CSDLAudioDriver.h"

#include "WallpaperEngine/Input/CInputContext.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"

#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Application {
using namespace WallpaperEngine::Data::Model;
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 *
 * @author Alexis Maiquez <almamu@almamu.com>
 */
class CWallpaperApplication {
  public:
    explicit CWallpaperApplication (CApplicationContext& context);

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
    [[nodiscard]] CApplicationContext& getContext () const;
    /**
     * Renders a frame
     */
    void update (Render::Drivers::Output::COutputViewport* viewport);
    /**
     * Gets the output
     */
    [[nodiscard]] const WallpaperEngine::Render::Drivers::Output::COutput& getOutput () const;

  private:
    /**
     * Sets up a combined container for the given background, adding default files and directories to the list
     *
     * @param container
     * @param bg
     */
    ContainerUniquePtr setupContainer (const std::string& bg) const;
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
    CApplicationContext& m_context;
    /** Maps screens to backgrounds */
    std::map<std::string, ProjectUniquePtr> m_backgrounds {};

    std::unique_ptr <WallpaperEngine::Audio::Drivers::Detectors::CAudioPlayingDetector> m_audioDetector = nullptr;
    std::unique_ptr <WallpaperEngine::Audio::CAudioContext> m_audioContext = nullptr;
    std::unique_ptr <WallpaperEngine::Audio::Drivers::CSDLAudioDriver> m_audioDriver = nullptr;
    std::unique_ptr <WallpaperEngine::Audio::Drivers::Recorders::CPlaybackRecorder> m_audioRecorder = nullptr;
    std::unique_ptr <WallpaperEngine::Render::CRenderContext> m_renderContext = nullptr;
    std::unique_ptr <WallpaperEngine::Render::Drivers::CVideoDriver> m_videoDriver = nullptr;
    std::unique_ptr <WallpaperEngine::Render::Drivers::Detectors::CFullScreenDetector> m_fullScreenDetector = nullptr;
    std::unique_ptr <WallpaperEngine::WebBrowser::CWebBrowserContext> m_browserContext = nullptr;
};
} // namespace WallpaperEngine::Application
