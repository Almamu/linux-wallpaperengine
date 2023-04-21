#pragma once

#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <string>

namespace WallpaperEngine::Render::Drivers
{
    class CVideoDriver
    {
    public:
        /**
         * @return The window handle used by this video driver
         */
        virtual void* getWindowHandle () const = 0;
        /**
         * @return The time that has passed since the driver started
         */
        virtual float getRenderTime () const = 0;
        /**
         * @return If a close was requested by the OS
         */
        virtual bool closeRequested () = 0;
        /**
         * @param size The new size for the window
         */
        virtual void resizeWindow (glm::ivec2 size) = 0;
        /**
         * @param size The new size and position of the window
         */
        virtual void resizeWindow (glm::ivec4 positionAndSize) = 0;
        /**
         * Shows the window created by the driver
         */
        virtual void showWindow () = 0;
        /**
         * Hides the window created by the driver
         */
        virtual void hideWindow () = 0;
        /**
         * @return The size of the framebuffer available for the driver
         */
        virtual glm::ivec2 getFramebufferSize () const = 0;
        /**
         * Performs buffer swapping
         */
        virtual void swapBuffers () = 0;
        /**
         * @return The number of rendered frames since the start of the driver
         */
        virtual uint32_t getFrameCounter () const = 0;
        /**
         * Wayland only: dispatch wayland event queue
        */
        virtual void dispatchEventQueue() const;
        /**
         * Wayland only: make EGL current
        */
        virtual void makeCurrent(const std::string& outputName) const;
        /**
         * Wayland only: whether an output should be rendered
        */
        virtual bool shouldRenderOutput(const std::string& outputName) const;
        /**
         * Wayland only: whether requires separate buffer flips on monitors
        */
        virtual bool requiresSeparateFlips() const;
        /**
         * Wayland only: flip output
        */
        virtual void swapOutputBuffer(const std::string& outputName);
        /**
         * Wayland only: gets currently rendered output
        */
        virtual std::string getCurrentlyRendered() const;
    };
}