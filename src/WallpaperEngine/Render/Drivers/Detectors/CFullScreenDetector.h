#pragma once

namespace WallpaperEngine::Render::Drivers::Detectors
{
    class CFullScreenDetector
    {
    public:
        /**
         * @return If anything is fullscreen
         */
        [[nodiscard]] virtual bool anythingFullscreen () const = 0;

        /**
         * Restarts the fullscreen detector, specially useful if there's any resources tied to the output driver
         */
        virtual void reset () = 0;
    };
}