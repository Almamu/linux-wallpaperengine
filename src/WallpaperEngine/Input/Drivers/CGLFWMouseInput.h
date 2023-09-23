#pragma once

#include "WallpaperEngine/Input/CMouseInput.h"

#include <glm/vec2.hpp>

namespace WallpaperEngine::Render::Drivers
{
    class CX11OpenGLDriver;
}

namespace WallpaperEngine::Input::Drivers
{
    /**
     * Handles mouse input for the background
     */
    class CGLFWMouseInput : public CMouseInput
    {
    public:
        explicit CGLFWMouseInput(Render::Drivers::CX11OpenGLDriver* driver);

        /**
         * Takes current mouse position and updates it
         */
        void update () override;

        /**
         * The virtual pointer's position
         */
        [[nodiscard]] glm::dvec2 position () const override;

    private:
        Render::Drivers::CX11OpenGLDriver* m_driver;

        /**
         * The current mouse position
         */
        glm::dvec2 m_mousePosition;
        glm::dvec2 m_reportedPosition;
    };
}

