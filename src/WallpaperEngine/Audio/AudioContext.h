#pragma once

#include <libavutil/samplefmt.h>
#include <vector>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PulseAudioPlaybackRecorder.h"

namespace WallpaperEngine {
namespace Application {
    class ApplicationContext;
}

namespace Audio {
    namespace Drivers {
	class AudioDriver;

	namespace Recorders {
	    class PulseAudioPlaybackRecorder;
	}
    } // namespace Drivers

    class AudioStream;

    class AudioContext {
    public:
	explicit AudioContext (Drivers::AudioDriver& driver);

	/**
	 * Registers the given stream in the driver for playing
	 *
	 * @param stream
	 */
	void addStream (AudioStream* stream) const;

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
	Application::ApplicationContext& getApplicationContext () const;
	/**
	 * @return The audio recorder to use to capture stereo mix data
	 */
	[[nodiscard]] Drivers::Recorders::PlaybackRecorder& getRecorder () const;

	/**
	 * @return The audio driver used to playback and record audio
	 */
	[[nodiscard]] Drivers::AudioDriver& getDriver () const;

    private:
	/** The audio driver in use */
	Drivers::AudioDriver& m_driver;
    };
} // namespace Audio
} // namespace WallpaperEngine