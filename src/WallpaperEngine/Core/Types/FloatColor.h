#pragma once

namespace WallpaperEngine::Core::Types
{
    class FloatColor
    {
    public:
        FloatColor (float r, float g, float b, float a) :
            r(r), g(g), b(b), a(a) { }

        /**
         * The red color
         */
        float r;
        /**
         * The green color
         */
        float g;
        /**
         * The blue color
         */
        float b;
        /**
         * The alpha
         */
        float a;
    };
}