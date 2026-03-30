#pragma once

#include "WallpaperEngine/Audio/AudioStream.h"

namespace WallpaperEngine {
class Context;
}

namespace WallpaperEngine::Audio {
class AudioStream;

namespace Drivers {
	/**
	 * Base class for audio driver implementations
	 */
	class AudioDriver {
	public:
		explicit AudioDriver (Context& applicationContext);

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

	private:
		Context& m_context;
	};
} // namespace Drivers
} // namespace WallpaperEngine::Audio