#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#include <GL/glew.h>

namespace WallpaperEngine::Assets
{
    class CTexture
    {
    private:
        enum ContainerVersion : int
        {
            UNKNOWN = -1,
            TEXB0003 = 3,
            TEXB0002 = 2,
            TEXB0001 = 1
        };

        enum TextureFormat : uint32_t
        {
            ARGB8888 = 0,
            DXT5 = 4,
            DXT3 = 6,
            DXT1 = 7,
            RG88 = 8,
            R8 = 9,
        };

        // extracted from the free image library
        enum FREE_IMAGE_FORMAT : int
        {
            FIF_UNKNOWN = -1,
            FIF_BMP		= 0,
            FIF_ICO		= 1,
            FIF_JPEG	= 2,
            FIF_JNG		= 3,
            FIF_KOALA	= 4,
            FIF_LBM		= 5,
            FIF_IFF     = FIF_LBM,
            FIF_MNG		= 6,
            FIF_PBM		= 7,
            FIF_PBMRAW	= 8,
            FIF_PCD		= 9,
            FIF_PCX		= 10,
            FIF_PGM		= 11,
            FIF_PGMRAW	= 12,
            FIF_PNG		= 13,
            FIF_PPM		= 14,
            FIF_PPMRAW	= 15,
            FIF_RAS		= 16,
            FIF_TARGA	= 17,
            FIF_TIFF	= 18,
            FIF_WBMP	= 19,
            FIF_PSD		= 20,
            FIF_CUT		= 21,
            FIF_XBM		= 22,
            FIF_XPM		= 23,
            FIF_DDS		= 24,
            FIF_GIF     = 25,
            FIF_HDR		= 26,
            FIF_FAXG3	= 27,
            FIF_SGI		= 28,
            FIF_EXR		= 29,
            FIF_J2K		= 30,
            FIF_JP2		= 31,
            FIF_PFM		= 32,
            FIF_PICT	= 33,
            FIF_RAW		= 34,
            FIF_WEBP	= 35,
            FIF_JXR     = 36
        };

        class TextureMipmap
        {
        public:
            TextureMipmap ();
            ~TextureMipmap ();

            /** Width of the mipmap */
            uint32_t width;
            /** Height of the mipmap */
            uint32_t height;
            /** If the mipmap data is compressed */
            uint32_t compression;
            /** Uncompressed size of the mipmap */
            uint32_t uncompressedSize;
            /** Compress size of the mipmap */
            uint32_t compressedSize;
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
    public:
        CTexture (void* fileData);
        ~CTexture ();

        const GLuint getTextureID () const;
        const TextureHeader* getHeader () const;


    private:
        void parseHeader (char* fileData);
        TextureMipmap* parseMipmap (TextureHeader* header, char** fileData);

        TextureHeader* m_header;
        GLuint m_textureID;
    };
}