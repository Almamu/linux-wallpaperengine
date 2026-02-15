#pragma once

#include <vector>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/AudioPlayingDetector.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"

namespace WallpaperEngine {
namespace Application {
    class ApplicationContext;
}

namespace Audio {
    class AudioStream;

    namespace Drivers {
	namespace Detectors {
	    class AudioPlayingDetector;
	}

	namespace Recorders {
	    class PulseAudioPlaybackRecorder;
	}

	/**
	 * Base class for audio driver implementations
	 */
	class AudioDriver {
	public:
	    explicit AudioDriver (
		Application::ApplicationContext& applicationContext, Detectors::AudioPlayingDetector& detector,
		Recorders::PlaybackRecorder& recorder
	    );

	    virtual ~AudioDriver () = default;
	    /**
	     * Registers the given stream in the driver for playing
	     *
	     * @param stream
	     */
	    virtual int addStream (AudioStream* stream) = 0;

	    /**
	     *
	     * @param streamId The stream to stop playing
	     */
	    virtual void removeStream (int streamId) = 0;

	    /**
	     * Updates status of the different audio settings
	     */
	    virtual void update ();

	    /**
	     * TODO: MAYBE THIS SHOULD BE OUR OWN DEFINITIONS INSTEAD OF LIBRARY SPECIFIC ONES?
	     *
	     * @return The audio format the driver supports
	     */
	    [[nodiscard]] virtual AVSampleFormat getFormat () const = 0;
	    /**
	     * @return The sample rate the driver supports
	     */
	    [[nodiscard]] virtual int getSampleRate () const = 0;
	    /**
	     * @return The channels the driver supports
	     */
	    [[nodiscard]] virtual int getChannels () const = 0;
	    /**
	     * @return The application context under which the audio driver is initialized
	     */
	    Application::ApplicationContext& getApplicationContext () const;
	    /**
	     * @return The audio playing detector to use to stop playing sound when something else starts playing
	     */
	    [[nodiscard]] Detectors::AudioPlayingDetector& getAudioDetector () const;
	    /**
	     * @return The audio recorder to use to capture stereo mix data
	     */
	    [[nodiscard]] Recorders::PlaybackRecorder& getRecorder () const;

	private:
	    Application::ApplicationContext& m_applicationContext;
	    Detectors::AudioPlayingDetector& m_detector;
	    Recorders::PlaybackRecorder& m_recorder;
	};
    } // namespace Drivers
} // namespace Audio
} // namespace WallpaperEngine