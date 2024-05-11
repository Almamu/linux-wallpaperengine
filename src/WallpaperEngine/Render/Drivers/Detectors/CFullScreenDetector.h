#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"

namespace WallpaperEngine::Render::Drivers::Detectors {
class CFullScreenDetector {
  public:
    explicit CFullScreenDetector (Application::CApplicationContext& appContext);
    virtual ~CFullScreenDetector () = default;

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
    [[nodiscard]] Application::CApplicationContext& getApplicationContext () const;

  private:
    Application::CApplicationContext& m_applicationContext;
};
} // namespace WallpaperEngine::Render::Drivers::Detectors