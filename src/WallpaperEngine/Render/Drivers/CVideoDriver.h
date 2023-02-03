#pragma once

#include <glm/vec2.hpp>

namespace WallpaperEngine::Render::Drivers
{
    class CVideoDriver
    {
    public:
        virtual float getRenderTime () = 0;
        virtual bool closeRequested () = 0;
        virtual void resizeWindow (glm::ivec2 size) = 0;
        virtual void showWindow () = 0;
        virtual void hideWindow () = 0;
        virtual glm::ivec2 getFramebufferSize () = 0;
        virtual void swapBuffers () = 0;
        virtual uint32_t getFrameCounter () = 0;
    };
}