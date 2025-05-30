#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Input/Drivers/CGLFWMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CFullScreenDetector.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace WallpaperEngine::Application {
class CApplicationContext;
class CWallpaperApplication;
} // namespace WallpaperEngine::Application

namespace WallpaperEngine::Render::Drivers {
using namespace WallpaperEngine::Application;

class CGLFWOpenGLDriver final : public CVideoDriver {
  public:
    explicit CGLFWOpenGLDriver (const char* windowTitle, CApplicationContext& context, CWallpaperApplication& app);
    ~CGLFWOpenGLDriver () override;

    [[nodiscard]] Output::COutput& getOutput () override;
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
    CApplicationContext& m_context;
    Input::Drivers::CGLFWMouseInput m_mouseInput;
    Output::COutput* m_output = nullptr;
    GLFWwindow* m_window = nullptr;
    uint32_t m_frameCounter = 0;
};
} // namespace WallpaperEngine::Render::Drivers