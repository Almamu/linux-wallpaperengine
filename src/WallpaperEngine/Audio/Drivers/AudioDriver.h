#pragma once

#include "WallpaperEngine/Context.h"

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
				Context& applicationContext, std::unique_ptr<Detectors::AudioPlayingDetector> detector,
				std::unique_ptr<Recorders::PlaybackRecorder> recorder
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
			Context& getContext () const;
			/**
			 * @return The audio playing detector to use to stop playing sound when something else starts playing
			 */
			[[nodiscard]] Detectors::AudioPlayingDetector& getAudioDetector () const;
			/**
			 * @return The audio recorder to use to capture stereo mix data
			 */
			[[nodiscard]] Recorders::PlaybackRecorder& getRecorder () const;

		private:
			Context& m_context;
			std::unique_ptr<Detectors::AudioPlayingDetector> m_detector;
			std::unique_ptr<Recorders::PlaybackRecorder> m_recorder;
		};
	} // namespace Drivers
} // namespace Audio
} // namespace WallpaperEngine