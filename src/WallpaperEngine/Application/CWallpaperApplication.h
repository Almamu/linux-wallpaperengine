#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"

#include "WallpaperEngine/Assets/CCombinedContainer.h"

#include "WallpaperEngine/Core/CProject.h"

#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Drivers/CX11OpenGLDriver.h"
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"
#endif

#include "WallpaperEngine/Render/Drivers/Detectors/CX11FullScreenDetector.h"
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/Detectors/CWaylandFullScreenDetector.h"
#endif

#include "WallpaperEngine/Render/Drivers/Output/CGLFWWindowOutput.h"
#include "WallpaperEngine/Render/Drivers/Output/CX11Output.h"
#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Render/Drivers/Output/CWaylandOutput.h"
#endif

#include "WallpaperEngine/Audio/Drivers/CSDLAudioDriver.h"

#include "WallpaperEngine/Input/CInputContext.h"
#include "WallpaperEngine/WebBrowsesr/CWebBrowserContext.h"

namespace WallpaperEngine::Application {
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 *
 * @author Alexis Maiquez <almamu@almamu.com>
 */
class CWallpaperApplication {
  public:
    explicit CWallpaperApplication (CApplicationContext& context, WallpaperEngine::WebBrowser::CWebBrowserContext& browserContext);
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
     * @return The default background to use if no specific project is loaded
     */
    [[nodiscard]] Core::CProject* getDefaultBackground () const;
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
     * Takes an screenshot of the background and saves it to the specified path
     *
     * @param context
     * @param filename
     * @param format
     */
    static void takeScreenshot (const Render::CRenderContext& context, const std::filesystem::path& filename,
                                FREE_IMAGE_FORMAT format);

    /** The default background to display if no specific background was loaded */
    Core::CProject* m_defaultBackground;
    /** The application context that contains the current app settings */
    CApplicationContext& m_context;
    /** Maps screens to backgrounds */
    std::map<std::string, Core::CProject*> m_backgrounds;

    WallpaperEngine::Audio::Drivers::Recorders::CPlaybackRecorder* audioRecorder;
    WallpaperEngine::Render::Drivers::CVideoDriver* videoDriver;
    WallpaperEngine::Input::CInputContext* inputContext;
    WallpaperEngine::Audio::Drivers::CSDLAudioDriver* audioDriver;
    WallpaperEngine::Render::CRenderContext* context;
    WallpaperEngine::Audio::CAudioContext* audioContext;
    WallpaperEngine::WebBrowser::CWebBrowserContext& browserContext;
};
} // namespace WallpaperEngine::Application
