#pragma once

#include <vector>

#include <irrlicht/irrlicht.h>
#include "CContext.h"

namespace WallpaperEngine::Irrlicht
{
    //!  Surface Loader for PNG files
    class CImageLoaderTex : public irr::video::IImageLoader
    {
    public:
        CImageLoaderTex (CContext* context);

        //! returns true if the file maybe is able to be loaded by this class
        //! based on the file extension (e.g. ".png")
        virtual bool isALoadableFileExtension(const irr::io::path& filename) const;

        //! returns true if the file maybe is able to be loaded by this class
        virtual bool isALoadableFileFormat(irr::io::IReadFile* file) const;

        //! creates a surface from the file
        virtual irr::video::IImage* loadImage(irr::io::IReadFile* input) const;

        virtual void loadImageFromARGB8Data (irr::video::IImage* output, const char* input, irr::u32 width, irr::u32 height, irr::u32 mipmap_width) const;

        virtual void loadImageFromDXT1 (irr::video::IImage* output, const char* input, irr::u32 destination_width, irr::u32 destination_height, irr::u32 origin_width, irr::u32 origin_height) const;
        virtual void loadImageFromDXT5 (irr::video::IImage* output, const char* input, irr::u32 destination_width, irr::u32 destination_height, irr::u32 origin_width, irr::u32 origin_height) const;
        virtual void loadImageFromDXT3 (irr::video::IImage* output, const char* input, irr::u32 destination_width, irr::u32 destination_height, irr::u32 origin_width, irr::u32 origin_height) const;
    private:
        enum ContainerVersion : int
        {
            UNKNOWN = -1,
            TEXB0003 = 3,
            TEXB0002 = 2,
            TEXB0001 = 1
        };

        enum TextureFormat : int
        {
            ARGB8888,
            RA88,
            A8,
            DXT5,
            DXT1,
            DXT3 = -1
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
            irr::u32 width;
            /** Height of the mipmap */
            irr::u32 height;
            /** If the mipmap data is compressed */
            irr::u32 compression;
            /** Uncompressed size of the mipmap */
            irr::u32 uncompressedSize;
            /** Compress size of the mipmap */
            irr::u32 compressedSize;
            /** Pointer to the compressed data */
            char* compressedData = nullptr;
            /** Pointer to the uncompressed data */
            char* uncompressedData = nullptr;
            /**
             * Performs actual decompression of the compressed data
             */
            void decompressData ();
        };

        class TextureContainer
        {
        public:
            TextureContainer ();
            ~TextureContainer ();

            /** The version of the texture container */
            ContainerVersion containerVersion = ContainerVersion::UNKNOWN;
            /** Real width of the texture */
            irr::u32 width;
            /** Real height of the texture */
            irr::u32 height;
            /** Texture width in memory (power of 2) */
            irr::u32 textureWidth;
            /** Texture height in memory (power of 2) */
            irr::u32 textureHeight;
            /** Texture data format */
            TextureFormat format;
            /** Free Image format */
            FREE_IMAGE_FORMAT freeimageFormat;
            /** Number of mipmap levels for the texture */
            irr::u32 mipmapCount;
            /** List of mipmaps */
            std::vector <TextureMipmap*> mipmaps;
        };

        /** Irrlicht context */
        CContext* m_context;

        /**
         * Parses the container file and returns a texture ready to be used
         *
         * @param input The file to parse the data from
         *
         * @return The texture ready to be used
         */
        irr::video::IImage* parseFile (irr::io::IReadFile* input) const;
        /**
         * Parses the container header and returns it's information
         *
         * @param input The file to parse the data from
         *
         * @return The container header data
         */
        TextureContainer* parseHeader (irr::io::IReadFile* input) const;
        /**
         * Parses a mipmap level and returns it's information
         *
         * @param header The container header where this mipmap is contained
         * @param input The file to parse the data from
         *
         * @return The mipmap info ready
         */
        TextureMipmap* parseMipmap (TextureContainer* header, irr::io::IReadFile* input) const;

        void BlockDecompressImageDXT1(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const;
        void DecompressBlockDXT1(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const;
        void BlockDecompressImageDXT3(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const;
        void DecompressBlockDXT3(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const;
        unsigned long PackRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) const;
        void BlockDecompressImageDXT5(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const;
        void DecompressBlockDXT5(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const;
    };
}
