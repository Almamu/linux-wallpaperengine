#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"

#include <string>

namespace WallpaperEngine::Render::Drivers::Detectors {
class FullScreenDetector {
public:
    explicit FullScreenDetector (Application::ApplicationContext& appContext);
    virtual ~FullScreenDetector () = default;

    /**
     * @return If anything is fullscreen
     */
    [[nodiscard]] virtual bool anythingFullscreen () const;
    /**
     * @return If isFullscreenOn can tell outputs apart, otherwise it only repeats anythingFullscreen
     */
    [[nodiscard]] virtual bool canTellOutputsApart () const;
    /**
     * Checks a single output. Only meaningful right after anythingFullscreen, which refreshes the state.
     * Detectors that cannot tell outputs apart report the global state.
     *
     * @param output The output's name (e.g. DP-1)
     * @return If anything is fullscreen on the given output
     */
    [[nodiscard]] virtual bool isFullscreenOn (const std::string& output) const;
    /**
     * Restarts the fullscreen detector, specially useful if there's any resources tied to the output driver
     */
    virtual void reset ();
    /**
     * @return The application context using this detector
     */
    [[nodiscard]] Application::ApplicationContext& getApplicationContext () const;

private:
    Application::ApplicationContext& m_applicationContext;
};
} // namespace WallpaperEngine::Render::Drivers::Detectors