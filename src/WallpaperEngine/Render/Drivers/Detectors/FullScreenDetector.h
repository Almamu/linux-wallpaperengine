#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"

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