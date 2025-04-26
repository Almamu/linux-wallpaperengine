#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CFullScreenDetector.h"

namespace WallpaperEngine {
namespace Application {
class CApplicationContext;
}

namespace Render::Drivers::Detectors {
class CFullScreenDetector;
}

namespace Audio::Drivers::Detectors {
/**
 * Base class for any implementation of audio playing detection
 */
class CAudioPlayingDetector {
  public:
    CAudioPlayingDetector (
        Application::CApplicationContext& appContext,
        const Render::Drivers::Detectors::CFullScreenDetector& fullscreenDetector);

    virtual ~CAudioPlayingDetector () = default;

    /**
     * @return If any kind of sound is currently playing on the default audio device
     */
    [[nodiscard]] bool anythingPlaying () const;

    /**
     * Updates the playing status to the specified value
     *
     * @param newState
     */
    void setIsPlaying (bool newState);

    /**
     * Checks if any audio is playing and updates state accordingly
     */
    virtual void update ();

  protected:
    /**
     * @return The application context using this detector
     */
    [[nodiscard]] Application::CApplicationContext& getApplicationContext ();
    /**
     * @return The fullscreen detector used
     */
    [[nodiscard]] const Render::Drivers::Detectors::CFullScreenDetector& getFullscreenDetector () const;

  private:
    bool m_isPlaying;

    Application::CApplicationContext& m_applicationContext;
    const Render::Drivers::Detectors::CFullScreenDetector& m_fullscreenDetector;
};
} // namespace Audio::Drivers::Detectors
} // namespace WallpaperEngine
