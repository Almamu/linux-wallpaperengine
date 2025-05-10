#pragma once

#include <glm/vec4.hpp>
#include <string>
#include <vector>

#include "CFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include <X11/Xlib.h>

namespace WallpaperEngine::Render::Drivers {
class CGLFWOpenGLDriver;

namespace Detectors {
class CX11FullScreenDetector final : public CFullScreenDetector {
  public:
    CX11FullScreenDetector (Application::CApplicationContext& appContext, CVideoDriver& driver);
    ~CX11FullScreenDetector () override;

    [[nodiscard]] bool anythingFullscreen () const override;
    void reset () override;

  private:
    void initialize ();
    void stop ();

    Display* m_display = nullptr;
    Window m_root;
    std::map<std::string, glm::ivec4> m_screens = {};
    CVideoDriver& m_driver;
};
} // namespace Detectors
} // namespace WallpaperEngine::Render::Drivers