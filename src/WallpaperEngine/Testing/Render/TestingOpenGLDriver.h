#pragma once

#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Input/Drivers/GLFWMouseInput.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

#include "WallpaperEngine/Testing/Input/TestingMouseInput.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace WallpaperEngine::Testing::Render {
using namespace WallpaperEngine::Render::Drivers;
using namespace WallpaperEngine::Testing::Input;

class TestingOpenGLDriver final : public VideoDriver {
public:
    explicit TestingOpenGLDriver (ApplicationContext& context, WallpaperApplication& app);
    ~TestingOpenGLDriver () override;

    [[nodiscard]] Output::Output& getOutput () override;
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
    Output::Output* m_output = nullptr;
    ApplicationContext& m_context;
    GLFWwindow* m_window = nullptr;
    TestingMouseInput m_mouseInput;
    uint32_t m_frameCounter = 0;
};
} // namespace WallpaperEngine::Testing::Render
