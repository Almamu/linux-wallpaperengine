#pragma once

#include <GL/glew.h>
#include <glm/vec4.hpp>

namespace WallpaperEngine::Assets
{
    class ITexture
    {
    public:
        enum TextureFormat : uint32_t
        {
            ARGB8888 = 0,
            DXT5 = 4,
            DXT3 = 6,
            DXT1 = 7,
            RG88 = 8,
            R8 = 9,
        };

        virtual const GLuint getTextureID () const = 0;
        virtual const uint32_t getTextureWidth () const = 0;
        virtual const uint32_t getTextureHeight () const = 0;
        virtual const uint32_t getRealWidth () const = 0;
        virtual const uint32_t getRealHeight () const = 0;
        virtual const TextureFormat getFormat () const = 0;
        virtual const glm::vec4* getResolution () const = 0;
    };
};