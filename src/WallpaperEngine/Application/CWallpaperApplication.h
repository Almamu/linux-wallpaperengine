#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"

#include "WallpaperEngine/Assets/CCombinedContainer.h"

#include "WallpaperEngine/Core/CProject.h"

#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Drivers/CGLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CFullScreenDetector.h"
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"
#endif

#ifdef ENABLE_X11
#include "WallpaperEngine/Render/Drivers/Detectors/CX11FullScreenDetector.h"
#endif
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/Detectors/CWaylandFullScreenDetector.h"
#endif

#include "WallpaperEngine/Render/Drivers/Output/CGLFWWindowOutput.h"
#ifdef ENABLE_X11
#include "WallpaperEngine/Render/Drivers/Output/CX11Output.h"
#endif
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/Output/CWaylandOutput.h"
#endif

#include "WallpaperEngine/Audio/Drivers/CSDLAudioDriver.h"

#include "WallpaperEngine/Input/CInputContext.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"

namespace WallpaperEngine::Application {
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 *
 * @author Alexis Maiquez <almamu@almamu.com>
 */
class CWallpaperApplication {
  public:
    explicit CWallpaperApplication (CApplicationContext& context);
    ~CWallpaperApplication ();

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
    [[nodiscard]] const std::map<std::string, Core::CProject*>& getBackgrounds () const;
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
    void setupContainer (CCombinedContainer& container, const std::string& bg) const;
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
    Core::CProject* loadBackground (const std::string& bg);
    /**
     * Prepares all background's values and updates their properties if required
     */
    void setupProperties ();
    /**
     * Updates the properties for the given background based on the current context
     *
     * @param project
     */
    void setupPropertiesForProject (const Core::CProject* project);
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
    std::map<std::string, Core::CProject*> m_backgrounds;

    WallpaperEngine::Audio::Drivers::Detectors::CAudioPlayingDetector* m_audioDetector;
    WallpaperEngine::Audio::CAudioContext* m_audioContext;
    WallpaperEngine::Audio::Drivers::CSDLAudioDriver* m_audioDriver;
    WallpaperEngine::Audio::Drivers::Recorders::CPlaybackRecorder* m_audioRecorder;
    WallpaperEngine::Input::CInputContext* m_inputContext;
    WallpaperEngine::Render::CRenderContext* m_renderContext;
    WallpaperEngine::Render::Drivers::CVideoDriver* m_videoDriver;
    WallpaperEngine::Render::Drivers::Detectors::CFullScreenDetector* m_fullScreenDetector;
    WallpaperEngine::WebBrowser::CWebBrowserContext* m_browserContext;
};
} // namespace WallpaperEngine::Application
