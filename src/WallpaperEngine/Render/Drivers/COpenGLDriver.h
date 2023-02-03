#pragma once

#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace WallpaperEngine::Render::Drivers
{
    class COpenGLDriver : public CVideoDriver
    {
    public:
        COpenGLDriver (const char* windowTitle);
        ~COpenGLDriver();

        float getRenderTime () override;
        bool closeRequested () override;
        void resizeWindow (glm::ivec2 size) override;
        void showWindow () override;
        void hideWindow () override;
        glm::ivec2 getFramebufferSize () override;
        void swapBuffers () override;
        uint32_t getFrameCounter () override;

        GLFWwindow* getWindow ();
    private:
        GLFWwindow* m_window;
        uint32_t m_frameCounter;
    };
}