#pragma once

#include <GL/glew.h>
#include <glm/vec4.hpp>
#include <vector>

namespace WallpaperEngine::Assets
{
    class ITexture
    {
    public:
        class TextureFrame
        {
        public:
            TextureFrame();
            ~TextureFrame();

            /** The image index of this frame */
            uint32_t frameNumber;
            /** The amount of time this frame spends being displayed */
            float frametime;
            /** The x position of the frame in the texture */
            float x;
            /** The y position of the frame in the texture */
            float y;
            /** The width of the frame in the texture */
            float width1;
            float width2;
            float height2;
            /** The height of the frame in the texture */
            float height1;
        };

        /**
         * Configures the data format for the texture
         */
        enum TextureFormat : uint32_t
        {
            ARGB8888 = 0,
            RGB888 = 1,
            RGB565 = 2,
            DXT5 = 4,
            DXT3 = 6,
            DXT1 = 7,
            RG88 = 8,
            R8 = 9,
            RG1616f = 10,
            R16f = 11,
            BC7 = 12,
            RGBa1010102 = 13,
            RGBA16161616f = 14,
            RGB161616f = 15,
        };

        /**
         * Configures how the texture will be handled by the background
         */
        enum TextureFlags : uint32_t
        {
            NoFlags = 0,
            NoInterpolation = 1,
            ClampUVs = 2,
            IsGif = 4,
        };

        virtual const GLuint getTextureID (uint32_t imageIndex = 0) const = 0;
        virtual const uint32_t getTextureWidth (uint32_t imageIndex = 0) const = 0;
        virtual const uint32_t getTextureHeight (uint32_t imageIndex = 0) const = 0;
        virtual const uint32_t getRealWidth () const = 0;
        virtual const uint32_t getRealHeight () const = 0;
        virtual const TextureFormat getFormat () const = 0;
        virtual const TextureFlags getFlags () const = 0;
        virtual const std::vector<TextureFrame*>& getFrames () const = 0;
        virtual const glm::vec4* getResolution () const = 0;
        virtual const bool isAnimated () const = 0;
    };
};