#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Input/Drivers/GLFWMouseInput.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace WallpaperEngine::Application {
class ApplicationContext;
class WallpaperApplication;
} // namespace WallpaperEngine::Application

namespace WallpaperEngine::Render::Drivers {
using namespace WallpaperEngine::Application;

class GLFWOpenGLDriver final : public VideoDriver {
  public:
    explicit GLFWOpenGLDriver (const char* windowTitle, ApplicationContext& context, WallpaperApplication& app);
    ~GLFWOpenGLDriver () override;

    [[nodiscard]] Output::Output& getOutput () override;
    [[nodiscard]] float getRenderTime () const override;
    bool closeRequested () override;
    void resizeWindow (glm::ivec2 size) override;
    void resizeWindow (glm::ivec4 sizeandpos) override;
    void showWindow () override;
    void hideWindow () override;
    [[nodiscard]] glm::ivec2 getFramebufferSize () const override;
    [[nodiscard]] uint32_t getFrameCounter () const override;
    void dispatchEventQueue () override;
    [[nodiscard]] void* getProcAddress (const char* name) const override;

    GLFWwindow* getWindow () const;

  private:
    ApplicationContext& m_context;
    Input::Drivers::GLFWMouseInput m_mouseInput;
    Output::Output* m_output = nullptr;
    GLFWwindow* m_window = nullptr;
    uint32_t m_frameCounter = 0;
};
} // namespace WallpaperEngine::Render::Drivers