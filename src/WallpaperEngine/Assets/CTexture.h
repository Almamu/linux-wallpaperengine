#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#include <GL/glew.h>
#include <glm/vec4.hpp>

#include <FreeImage.h>

namespace WallpaperEngine::Assets
{
    class CTexture
    {
        struct TextureHeader;

    public:
        CTexture (void* fileData);
        ~CTexture ();

        const GLuint getTextureID () const;
        const TextureHeader* getHeader () const;
        const glm::vec4* getResolution () const;


        enum TextureFormat : uint32_t
        {
            ARGB8888 = 0,
            DXT5 = 4,
            DXT3 = 6,
            DXT1 = 7,
            RG88 = 8,
            R8 = 9,
        };
    private:
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
        enum TextureFlags: uint32_t
        {
            None = 0,
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