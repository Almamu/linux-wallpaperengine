#pragma once

#include "../../../../../include/frontends/configuration.h"

namespace WallpaperEngine::Render::Drivers::Detectors {
class FullScreenDetector {
public:
	explicit FullScreenDetector (wp_rendering_pause_check& detection);
	virtual ~FullScreenDetector () = default;

	/**
	 * @return If anything is fullscreen
	 */
	[[nodiscard]] virtual bool anythingFullscreen () const;

private:
	wp_rendering_pause_check& m_detection;
};
} // namespace WallpaperEngine::Render::Drivers::Detectors