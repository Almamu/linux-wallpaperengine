#pragma once

#ifdef ENABLE_WAYLAND
#include "../Render/Drivers/CWaylandOpenGLDriver.h"
#endif

#include <glm/vec2.hpp>
#include "GLFW/glfw3.h"


namespace WallpaperEngine::Input
{
    /**
     * Handles mouse input for the background
     */
    class CMouseInput
    {
    public:
        explicit CMouseInput(GLFWwindow* window);
#ifdef ENABLE_WAYLAND
        explicit CMouseInput(WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver);
#endif

        /**
         * Takes current mouse position and updates it
         */
        void update ();

        /**
         * The virtual pointer's position
         */
        glm::dvec2 position;

    private:
        /**
         * The GLFW window to get mouse position from
         */
        GLFWwindow* m_window = nullptr;

        /**
         * The current mouse position
         */
        glm::dvec2 m_mousePosition;

#ifdef ENABLE_WAYLAND
        /**
         * Wayland: Driver
        */
        WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* waylandDriver = nullptr;
#endif
    };
}

