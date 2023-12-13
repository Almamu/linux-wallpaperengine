#pragma once

#include <libavutil/samplefmt.h>
#include <vector>

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/CPulseAudioPlaybackRecorder.h"

namespace WallpaperEngine {
namespace Application {
class CApplicationContext;
}

namespace Audio {
namespace Drivers {
class CAudioDriver;

namespace Recorders {
class CPulseAudioPlaybackRecorder;
}
} // namespace Drivers

class CAudioStream;

class CAudioContext {
  public:
    explicit CAudioContext (Drivers::CAudioDriver& driver);

    /**
     * Registers the given stream in the driver for playing
     *
     * @param stream
     */
    void addStream (CAudioStream* stream);

    /**
     * TODO: MAYBE THIS SHOULD BE OUR OWN DEFINITIONS INSTEAD OF LIBRARY SPECIFIC ONES?
     *
     * @return The audio format the driver supports
     */
    [[nodiscard]] AVSampleFormat getFormat () const;
    /**
     * @return The sample rate the driver supports
     */
    [[nodiscard]] int getSampleRate () const;
    /**
     * @return The channels the driver supports
     */
    [[nodiscard]] int getChannels () const;
    /**
     * @return The application context under which the audio driver is initialized
     */
    Application::CApplicationContext& getApplicationContext ();
    /**
     * @return The audio recorder to use to capture stereo mix data
     */
    [[nodiscard]] Drivers::Recorders::CPlaybackRecorder& getRecorder ();

  private:
    /** The audio driver in use */
    Drivers::CAudioDriver& m_driver;
};
} // namespace Audio
} // namespace WallpaperEngine