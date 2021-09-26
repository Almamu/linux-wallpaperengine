#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#include <GL/glew.h>
#include <glm/vec4.hpp>

#include <FreeImage.h>
#include "ITexture.h"

namespace WallpaperEngine::Assets
{
    class CTexture : public ITexture
    {
        struct TextureHeader;

    public:
        CTexture (void* fileData);
        ~CTexture ();

        const GLuint getTextureID () const override;
        const uint32_t getTextureWidth () const override;
        const uint32_t getTextureHeight () const override;
        const uint32_t getRealWidth () const override;
        const uint32_t getRealHeight () const override;
        const TextureFormat getFormat () const override;
        const glm::vec4* getResolution () const override;

    private:
        const TextureHeader* getHeader () const;

        enum ContainerVersion : int
        {
            UNKNOWN = -1,
            TEXB0003 = 3,
            TEXB0002 = 2,
            TEXB0001 = 1
        };

        class TextureMipmap
        {
        public:
            TextureMipmap ();
            ~TextureMipmap ();

            /** Width of the mipmap */
            uint32_t width = 0;
            /** Height of the mipmap */
            uint32_t height = 0;
            /** If the mipmap data is compressed */
            uint32_t compression = 0;
            /** Uncompressed size of the mipmap */
            uint32_t uncompressedSize = 0;
            /** Compress size of the mipmap */
            uint32_t compressedSize = 0;
            /** Pointer to the compressed data */
            char* compressedData = nullptr;
            /** Pointer to the uncompressed data */
            char* uncompressedData = nullptr;
            /**
             * Performs actual decompression of the compressed data
             */
            void decompressData ();
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

        class TextureHeader
        {
        public:
            TextureHeader ();
            ~TextureHeader ();

            /** The version of the texture container */
            ContainerVersion containerVersion = ContainerVersion::UNKNOWN;
            /** Flags with extra texture information */
            TextureFlags flags;
            /** Real width of the texture */
            uint32_t width;
            /** Real height of the texture */
            uint32_t height;
            /** Texture width in memory (power of 2) */
            uint32_t textureWidth;
            /** Texture height in memory (power of 2) */
            uint32_t textureHeight;
            /** Texture data format */
            TextureFormat format;
            /** Free Image format */
            FREE_IMAGE_FORMAT freeImageFormat = FREE_IMAGE_FORMAT::FIF_UNKNOWN;
            /** Number of mipmap levels on the texture */
            uint32_t mipmapCount;
            /** List of mipmaps */
            std::vector <TextureMipmap*> mipmaps;
        };
    private:
        void parseHeader (char* fileData);
        TextureMipmap* parseMipmap (TextureHeader* header, char** fileData);

        TextureHeader* m_header;
        GLuint m_textureID;
        glm::vec4 m_resolution;
    };
}