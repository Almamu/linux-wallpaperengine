#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CFullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CX11FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/Output/CX11Output.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"

namespace WallpaperEngine::Application
{
    class CApplicationContext;
    class CWallpaperApplication;
}

namespace WallpaperEngine::Render::Drivers
{
    using namespace WallpaperEngine::Application;

    class CX11OpenGLDriver : public CVideoDriver
    {
    public:
        explicit CX11OpenGLDriver (const char* windowTitle, CApplicationContext& context, CWallpaperApplication& app);
        ~CX11OpenGLDriver();

        [[nodiscard]] Detectors::CFullScreenDetector& getFullscreenDetector () override;
        [[nodiscard]] Output::COutput& getOutput () override;
        [[nodiscard]] float getRenderTime () const override;
        bool closeRequested () override;
        void resizeWindow (glm::ivec2 size) override;
        void resizeWindow (glm::ivec4 sizeandpos) override;
        void showWindow () override;
        void hideWindow () override;
        [[nodiscard]] glm::ivec2 getFramebufferSize () const override;
        void swapBuffers () override;
        [[nodiscard]] uint32_t getFrameCounter () const override;
        void dispatchEventQueue() const override;
        [[nodiscard]] void* getProcAddress (const char* name) const override;

        GLFWwindow* getWindow ();

    private:
        Detectors::CX11FullScreenDetector m_fullscreenDetector;
        CApplicationContext& m_context;
        Output::COutput* m_output;
        GLFWwindow* m_window;
        uint32_t m_frameCounter;
    };
}