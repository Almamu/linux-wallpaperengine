#pragma once

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
        CMouseInput(GLFWwindow* window);

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
        GLFWwindow* m_window;

        /**
         * The current mouse position
         */
        glm::dvec2 m_mousePosition;
    };
};

