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

    class COpenGLDriver : public CVideoDriver
    {
    public:
        explicit COpenGLDriver (const char* windowTitle);
        ~COpenGLDriver();

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