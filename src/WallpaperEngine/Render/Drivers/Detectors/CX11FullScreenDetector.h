#pragma once

#include <glm/vec4.hpp>
#include <string>
#include <vector>

#include "CFullScreenDetector.h"
#include <X11/Xlib.h>

namespace WallpaperEngine::Render::Drivers {
class CGLFWOpenGLDriver;

namespace Detectors {
class CX11FullScreenDetector final : public CFullScreenDetector {
  public:
    CX11FullScreenDetector (Application::CApplicationContext& appContext, CGLFWOpenGLDriver& driver);
    ~CX11FullScreenDetector () override;

    [[nodiscard]] bool anythingFullscreen () const override;
    void reset () override;

  private:
    void initialize ();
    void stop ();

    struct ScreenInfo {
        glm::ivec4 viewport;
        std::string name;
    };

    Display* m_display;
    Window m_root;
    std::vector<ScreenInfo> m_screens;
    CGLFWOpenGLDriver& m_driver;
};
} // namespace Detectors
} // namespace WallpaperEngine::Render::Drivers