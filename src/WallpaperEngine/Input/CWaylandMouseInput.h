#pragma once

#include "CMouseInput.h"
#include "../Render/Drivers/CWaylandOpenGLDriver.h"

#include <glm/vec2.hpp>
#include "GLFW/glfw3.h"


namespace WallpaperEngine::Input
{
    /**
     * Handles mouse input for the background
     */
    class CWaylandMouseInput : public CMouseInput
    {
    public:
        explicit CWaylandMouseInput(WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* driver);

        /**
         * Takes current mouse position and updates it
         */
        void update () override;

        /**
         * The virtual pointer's position
         */
        glm::dvec2 position () const override;

    private:
        /**
         * Wayland: Driver
        */
        WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver* waylandDriver = nullptr;

        glm::dvec2 pos;
    };
}

