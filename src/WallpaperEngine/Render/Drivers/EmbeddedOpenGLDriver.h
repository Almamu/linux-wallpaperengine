#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Input/Drivers/EmbeddedMouseInput.h"
#include "WallpaperEngine/Render/Drivers/EmbeddedHost.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

namespace WallpaperEngine::Application {
class ApplicationContext;
class WallpaperApplication;
} // namespace WallpaperEngine::Application

namespace WallpaperEngine::Render::Drivers {
using namespace WallpaperEngine::Application;

/**
 * Video driver for running the renderer inside a host that already owns an
 * OpenGL context, such as a Qt Quick item living in plasmashell.
 *
 * Unlike every other driver this one creates no window, no context and no event
 * loop. The host drives the frame clock by calling dispatchEventQueue() with its
 * context current, and the renderer draws straight into the host's framebuffer.
 */
class EmbeddedOpenGLDriver final : public VideoDriver {
public:
    EmbeddedOpenGLDriver (ApplicationContext& context, WallpaperApplication& app, EmbeddedHost& host);
    ~EmbeddedOpenGLDriver () override;

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

private:
    ApplicationContext& m_context;
    EmbeddedHost& m_host;
    Input::Drivers::EmbeddedMouseInput m_mouseInput;
    Output::Output* m_output = nullptr;
    uint32_t m_frameCounter = 0;
};
} // namespace WallpaperEngine::Render::Drivers
