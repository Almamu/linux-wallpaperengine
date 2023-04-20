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
        /**
         * Takes current mouse position and updates it
         */
        virtual void update () = 0;

        /**
         * The virtual pointer's position
         */
        virtual glm::dvec2 position () const = 0;
    };
}

