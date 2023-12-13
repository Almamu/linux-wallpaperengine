#pragma once

#ifdef ENABLE_WAYLAND

#include <glm/vec4.hpp>
#include <string>
#include <vector>

#include "CFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"

namespace WallpaperEngine::Render::Drivers {
class CWaylandOpenGLDriver;

namespace Detectors {
class CWaylandFullScreenDetector final : public CFullScreenDetector {
  public:
    CWaylandFullScreenDetector (Application::CApplicationContext& appContext, CWaylandOpenGLDriver& driver);
    ~CWaylandFullScreenDetector () override = default;

    [[nodiscard]] bool anythingFullscreen () const override;
    void reset () override;
};
} // namespace Detectors
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */