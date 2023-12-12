#pragma once

#include <GL/glew.h>
#include <glm/vec4.hpp>
#include <vector>

namespace WallpaperEngine::Assets
{
    /**
     * Base interface that describes the minimum information required for a texture
     * to be displayed by the engine
     */
    class ITexture
    {
    public:
        /**
         * Texture frame information for animated textures
         */
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
         * Data formats for textures in memory
         */
        enum TextureFormat : uint32_t
        {
            UNKNOWN = 0xFFFFFFFF,
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
         * Different settings of the textures
         */
        enum TextureFlags : uint32_t
        {
            NoFlags = 0,
            NoInterpolation = 1,
            ClampUVs = 2,
            IsGif = 4,
            ClampUVsBorder = 8,
        };

        /**
         * @param imageIndex For animated textures, the frame to get the ID of
         * @return The OpenGL texture to use when rendering
         */
        [[nodiscard]] virtual const GLuint getTextureID (uint32_t imageIndex = 0) const = 0;
        /**
         * @param imageIndex For animated textures, the frame to get the ID of
         * @return The texture's width
         */
        [[nodiscard]] virtual const uint32_t getTextureWidth (uint32_t imageIndex = 0) const = 0;
        /**
         * @param imageIndex For animated textures, the frame to get the ID of
         * @return The texture's height
         */
        [[nodiscard]] virtual const uint32_t getTextureHeight (uint32_t imageIndex = 0) const = 0;
        /**
         * @return The textures real width
         */
        [[nodiscard]] virtual const uint32_t getRealWidth () const = 0;
        /**
         * @return The textures real height
         */
        [[nodiscard]] virtual const uint32_t getRealHeight () const = 0;
        /**
         * @return The texture's memory format
         */
        [[nodiscard]] virtual const TextureFormat getFormat () const = 0;
        /**
         * @return The texture's settings
         */
        [[nodiscard]] virtual const TextureFlags getFlags () const = 0;
        /**
         * @return The list of frames this texture has
         */
        [[nodiscard]] virtual const std::vector<TextureFrame*>& getFrames () const = 0;
        /**
         * @return The texture's resolution vector
         */
        [[nodiscard]] virtual const glm::vec4* getResolution () const = 0;
        /**
         * @return If the texture is animated or not
         */
        [[nodiscard]] virtual const bool isAnimated () const = 0;
    };
}