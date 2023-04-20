#pragma once

#include "CMouseInput.h"

#include <glm/vec2.hpp>
#include "GLFW/glfw3.h"

namespace WallpaperEngine::Input
{
    /**
     * Handles mouse input for the background
     */
    class CGLFWMouseInput : public CMouseInput
    {
    public:
        explicit CGLFWMouseInput(GLFWwindow* window);

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
         * The GLFW window to get mouse position from
         */
        GLFWwindow* m_window = nullptr;

        /**
         * The current mouse position
         */
        glm::dvec2 m_mousePosition;
        glm::dvec2 m_reportedPosition;
    };
}

