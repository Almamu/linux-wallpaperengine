#pragma once

#include <memory>

#include <libavutil/samplefmt.h>

#include "Drivers/AudioDriver.h"
#include "PlaybackRecorder.h"
#include "AudioPlayingDetector.h"

namespace WallpaperEngine::Audio {
	namespace Drivers {
		class AudioDriver;
	}

	class AudioStream;

	class AudioContext {
	public:
		explicit AudioContext (
			std::unique_ptr<Drivers::AudioDriver> driver,
			std::unique_ptr<PlaybackRecorder> recorder,
			std::unique_ptr<AudioPlayingDetector> detector
		);

		/**
		 * Registers the given stream in the driver for playing
		 *
		 * @param stream
		 */
		int addStream (AudioStream* stream) const;

		/**
		 * @param streamId The stream to stop playing
		 */
		void removeStream (int streamId) const;

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
		 * @return The audio recorder to use to capture stereo mix data
		 */
		[[nodiscard]] PlaybackRecorder& getRecorder () const;
		/**
	     * @return The audio playing detector to use to detect when audio is playing from something else
	     */
		[[nodiscard]] AudioPlayingDetector& getDetector () const;
		/**
		 * @return The audio driver used to playback and record audio
		 */
		[[nodiscard]] Drivers::AudioDriver& getDriver () const;

	private:
		std::unique_ptr<PlaybackRecorder> m_recorder;
		std::unique_ptr<AudioPlayingDetector> m_detector;
		/** The audio driver in use */
		std::unique_ptr<Drivers::AudioDriver> m_driver;
	};
} // namespace Audio