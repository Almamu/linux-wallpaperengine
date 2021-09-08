#pragma once

namespace WallpaperEngine::Core::Types
{
    class IntegerColor
    {
    public:
        IntegerColor (uint8_t r, uint8_t g, uint8_t b, uint8_t a) :
            r(r), g(g), b(b), a(a) { }

        /**
         * The red color
         */
        uint8_t r;
        /**
         * The green color
         */
        uint8_t g;
        /**
         * The blue color
         */
        uint8_t b;
        /**
         * The alpha
         */
        uint8_t a;
    };
}