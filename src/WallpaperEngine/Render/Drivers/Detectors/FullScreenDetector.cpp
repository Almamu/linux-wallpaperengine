#include "FullScreenDetector.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Drivers::Detectors;

FullScreenDetector::FullScreenDetector (wp_rendering_pause_check& detection) : m_detection (detection) { }

bool FullScreenDetector::anythingFullscreen () const { return m_detection.is_paused (m_detection.user_parameter); }
