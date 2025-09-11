#pragma once

#include <glm/vec4.hpp>
#include <string>
#include <vector>

#include "FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"
#include <X11/Xlib.h>

namespace WallpaperEngine::Render::Drivers {
class GLFWOpenGLDriver;

namespace Detectors {
class X11FullScreenDetector final : public FullScreenDetector {
  public:
    X11FullScreenDetector (Application::ApplicationContext& appContext, VideoDriver& driver);
    ~X11FullScreenDetector () override;

    [[nodiscard]] bool anythingFullscreen () const override;
    void reset () override;

  private:
    void initialize ();
    void stop ();

    Display* m_display = nullptr;
    Window m_root;
    std::map<std::string, glm::ivec4> m_screens = {};
    VideoDriver& m_driver;
};
} // namespace Detectors
} // namespace WallpaperEngine::Render::Drivers