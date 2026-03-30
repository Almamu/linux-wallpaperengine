#pragma once

namespace WallpaperEngine {
class Context;
}

namespace WallpaperEngine::Audio {
/**
 * Base class for any implementation of audio playing detection
 */
class AudioPlayingDetector final {
public:
	explicit AudioPlayingDetector (Context& context);

	/**
	 * @return If any kind of sound is currently playing on the default audio device
	 */
	[[nodiscard]] bool anythingPlaying () const;

	/**
	 * Checks if any audio is playing and updates state accordingly
	 */
	void update ();

private:
	Context& m_context;

	bool m_isPlaying = false;
};
} // namespace WallpaperEngine::Audio
