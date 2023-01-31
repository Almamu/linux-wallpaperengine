#pragma once

namespace WallpaperEngine::Core::Types
{
    class FloatColor
    {
    public:
        FloatColor (double r, double g, double b, double a) :
            r(r), g(g), b(b), a(a) { }

        /**
         * The red color
         */
        double r;
        /**
         * The green color
         */
        double g;
        /**
         * The blue color
         */
        double b;
        /**
         * The alpha
         */
        double a;
    };
}