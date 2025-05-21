#pragma once

#include "WallpaperEngine/Input/Drivers/CGLFWMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"

#include "WallpaperEngine/Testing/Input/CTestingMouseInput.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace WallpaperEngine::Testing::Render {
using namespace WallpaperEngine::Render::Drivers;
using namespace WallpaperEngine::Testing::Input;

class CTestingOpenGLDriver final  : public CVideoDriver {
  public:
    explicit CTestingOpenGLDriver (CApplicationContext& context, CWallpaperApplication& app);
    ~CTestingOpenGLDriver () override;

    [[nodiscard]] Output::COutput & getOutput() override;
    [[nodiscard]] void* getProcAddress (const char* name) const override;
    [[nodiscard]] float getRenderTime () const override;
    bool closeRequested () override;
    void resizeWindow (glm::ivec2 size) override;
    void resizeWindow (glm::ivec4 sizeandpos) override;
    void showWindow () override;
    void hideWindow () override;
    [[nodiscard]] glm::ivec2 getFramebufferSize () const override;
    [[nodiscard]] uint32_t getFrameCounter () const override;
    void dispatchEventQueue () override;

  private:
    Output::COutput* m_output = nullptr;
    CApplicationContext& m_context;
    GLFWwindow* m_window = nullptr;
    CTestingMouseInput m_mouseInput;
    uint32_t m_frameCounter = 0;
};
} // namespace WallpaperEngine::Testing::Render
