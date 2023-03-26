#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include "WallpaperEngine/Application/CApplicationContext.h"

namespace WallpaperEngine::Application
{
    class CApplicationContext;
}

namespace WallpaperEngine::Render::Drivers
{
    using namespace WallpaperEngine::Application;

    class CX11OpenGLDriver : public CVideoDriver
    {
    public:
        explicit CX11OpenGLDriver (const char* windowTitle, CApplicationContext& context);
        ~CX11OpenGLDriver();

        void* getWindowHandle () const;
        float getRenderTime () const override;
        bool closeRequested () override;
        void resizeWindow (glm::ivec2 size) override;
        void resizeWindow (glm::ivec4 sizeandpos) override;
        void showWindow () override;
        void hideWindow () override;
        glm::ivec2 getFramebufferSize () const override;
        void swapBuffers () override;
        uint32_t getFrameCounter () const override;

        GLFWwindow* getWindow ();

    private:
        GLFWwindow* m_window;
        uint32_t m_frameCounter;
    };
}