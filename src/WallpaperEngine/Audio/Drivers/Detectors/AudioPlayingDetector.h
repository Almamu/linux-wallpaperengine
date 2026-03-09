#pragma once

#include <memory>

#include "../../../../../include/frontends/configuration.h"

#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"

namespace WallpaperEngine {
namespace Application {
	class ApplicationContext;
}

namespace Audio::Drivers::Detectors {
	/**
	 * Base class for any implementation of audio playing detection
	 */
	class AudioPlayingDetector {
	public:
		AudioPlayingDetector (
			wp_mute_check& source, std::unique_ptr<Render::Drivers::Detectors::FullScreenDetector> fullscreenDetector
		);

		virtual ~AudioPlayingDetector () = default;

		/**
		 * @return If any kind of sound is currently playing on the default audio device
		 */
		[[nodiscard]] bool anythingPlaying () const;

		/**
		 * Checks if any audio is playing and updates state accordingly
		 */
		virtual void update ();

	protected:
		/**
		 * @return The fullscreen detector used
		 */
		[[nodiscard]] const Render::Drivers::Detectors::FullScreenDetector& getFullscreenDetector () const;

	private:
		wp_mute_check& m_source;

		bool m_isPlaying = false;

		std::unique_ptr<Render::Drivers::Detectors::FullScreenDetector> m_fullscreenDetector;
	};
} // namespace Audio::Drivers::Detectors
} // namespace WallpaperEngine
