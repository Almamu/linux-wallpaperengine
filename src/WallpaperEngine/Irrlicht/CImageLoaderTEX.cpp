#include "CImageLoaderTEX.h"

#include <irrlicht/irrlicht.h>
#include <lz4.h>
#include <string>
#include <stdexcept>

using namespace WallpaperEngine::Irrlicht;

namespace WallpaperEngine::Irrlicht
{
    CImageLoaderTex::TextureMipmap::TextureMipmap ()
    {
    }

    CImageLoaderTex::TextureMipmap::~TextureMipmap ()
    {
        if (this->compression == 1)
            delete this->compressedData;

        delete this->uncompressedData;
    }

    void CImageLoaderTex::TextureMipmap::decompressData ()
    {
        if (this->compression == 1)
        {
            this->uncompressedData = new char [this->uncompressedSize];

            int result = LZ4_decompress_safe (
                this->compressedData, this->uncompressedData,
                this->compressedSize, this->uncompressedSize
            );

            if (!result)
            {
                throw std::runtime_error ("Cannot decompress texture data");
            }
        }
    }

    CImageLoaderTex::TextureContainer::TextureContainer () :
        freeimageFormat (FREE_IMAGE_FORMAT::FIF_UNKNOWN)
    {
    }

    CImageLoaderTex::TextureContainer::~TextureContainer ()
    {
        auto cur = this->mipmaps.begin ();
        auto end = this->mipmaps.end ();

        for (; cur != end; cur ++)
        {
            delete *cur;
        }
    }

    CImageLoaderTex::CImageLoaderTex (CContext* context) :
        m_context (context)
    {
    }

    //! returns true if the file maybe is able to be loaded by this class
    //! based on the file extension (e.g. ".tga")
    bool CImageLoaderTex::isALoadableFileExtension (const irr::io::path &filename) const
    {
        return irr::core::hasFileExtension (filename, "tex");
    }


    //! returns true if the file maybe is able to be loaded by this class
    bool CImageLoaderTex::isALoadableFileFormat (irr::io::IReadFile *file) const
    {
        return false;
    }

    CImageLoaderTex::TextureContainer* CImageLoaderTex::parseHeader (irr::io::IReadFile* input) const
    {
        char buffer [1024];

        if (input->read (buffer, 9) != 9 || memcmp (buffer, "TEXV0005", 9) != 0)
        {
            throw std::runtime_error ("unexpected container type");
        }

        if (input->read (buffer, 9) != 9 || memcmp (buffer, "TEXI0001", 9) != 0)
        {
            throw std::runtime_error ("unexpected sub-container type");
        }

        TextureContainer* header = new TextureContainer;

        input->read (&header->format, 4);
        input->seek (4, true); // ignore bytes
        input->read (&header->textureWidth, 4);
        input->read (&header->textureHeight, 4);
        input->read (&header->width, 4);
        input->read (&header->height, 4);
        input->seek (4, true); // ignore bytes
        input->read (buffer, 9);

        if (memcmp (buffer, "TEXB0003", 9) == 0)
        {
            header->containerVersion = ContainerVersion::TEXB0003;

            input->seek (4, true);
            input->read (&header->freeimageFormat, 4);
        }
        else if (memcmp (buffer, "TEXB0002", 9) == 0)
        {
            header->containerVersion = ContainerVersion::TEXB0002;

            input->seek (4, true);
        }
        else if (memcmp (buffer, "TEXB0001", 9) == 0)
        {
            header->containerVersion = ContainerVersion::TEXB0001;

            input->seek (4, true);
        }
        else
        {
            delete header;

            throw std::runtime_error ("Unknown container type");
        }

        if (header->format == TextureFormat::A8)
        {
            delete header;

            throw std::runtime_error ("A8 format not supported yet");
        }

        if (header->format == TextureFormat::RA88)
        {
            delete header;

            throw std::runtime_error ("RA88 format not supported yet");
        }

        input->read (&header->mipmapCount, 4);

        for (irr::u32 i = 0; i < header->mipmapCount; i ++)
        {
            header->mipmaps.push_back (
                this->parseMipmap (header, input)
            );
        }

        return header;
    }

    CImageLoaderTex::TextureMipmap* CImageLoaderTex::parseMipmap(TextureContainer* header, irr::io::IReadFile* input) const
    {
        TextureMipmap* mipmap = new TextureMipmap ();

        input->read (&mipmap->width, 4);
        input->read (&mipmap->height, 4);

        if (header->containerVersion == ContainerVersion::TEXB0002 ||
            header->containerVersion == ContainerVersion::TEXB0003)
        {
            input->read (&mipmap->compression, 4);
            input->read (&mipmap->uncompressedSize, 4);
        }

        input->read (&mipmap->compressedSize, 4);

        // TODO: BETTER POSITION FOR THIS
        if (mipmap->compression == 0)
        {
            // this might be better named as mipmap_bytes_size instead of compressed_size
            // as in uncompressed files this variable actually holds the file length
            mipmap->uncompressedSize = mipmap->compressedSize;
        }

        mipmap->uncompressedData = new char [mipmap->uncompressedSize];

        if (mipmap->compression == 1)
        {
            mipmap->compressedData = new char [mipmap->compressedSize];

            input->read (mipmap->compressedData, mipmap->compressedSize);

            mipmap->decompressData ();
        }
        else
        {
            input->read (mipmap->uncompressedData, mipmap->uncompressedSize);
        }

        return mipmap;
    }

    irr::video::IImage* CImageLoaderTex::parseFile (irr::io::IReadFile* input) const
    {
        irr::video::IImage *image = nullptr;
        TextureContainer* header = nullptr;
        TextureMipmap* mipmap = nullptr;

        if (input == nullptr)
            return nullptr;

        header = this->parseHeader (input);
        mipmap = *header->mipmaps.begin ();

        if (header->freeimageFormat == FREE_IMAGE_FORMAT::FIF_UNKNOWN)
        {
            image = this->m_context->getDevice ()->getVideoDriver ()->createImage (
                    irr::video::ECF_A8R8G8B8, irr::core::dimension2d<irr::u32> (header->width, header->height)
            );

            if (image == nullptr)
            {
                delete header;

                throw std::runtime_error ("cannot create destination image");
            }

            switch (header->format)
            {
                case TextureFormat::ARGB8888:
                    this->loadImageFromARGB8Data (
                            image, mipmap->uncompressedData, header->width, header->height, mipmap->width
                    );
                    break;
                case TextureFormat::DXT5:
                    this->loadImageFromDXT5 (
                            image, mipmap->uncompressedData, header->width, header->height, mipmap->width, mipmap->height
                    );
                    break;
                case TextureFormat::DXT1:
                    this->loadImageFromDXT1 (
                            image, mipmap->uncompressedData, header->width, header->height, mipmap->width, mipmap->height
                    );
                    break;
                case TextureFormat::DXT3:
                    delete header;

                    throw std::runtime_error ("DXT3 textures not supported yet");
            }
        }
        else
        {
            // generate a memory file for irrlicht to load the proper one as free image files
            // that are used by wallpaperengine are usually supported formats in irrlicht
            // TODO: FIND A WAY TO CALL THE PROPER LOADER DIRECTLY INSTEAD OF GENERATING A FILE?
            char tmpname [TMP_MAX];

            std::tmpnam (tmpname);
            std::string filename = tmpname;
            irr::io::IReadFile* file = nullptr;

            switch (header->freeimageFormat)
            {
                case FREE_IMAGE_FORMAT::FIF_BMP:
                    filename += ".bmp";
                    break;
                case FREE_IMAGE_FORMAT::FIF_PNG:
                    filename += ".png";
                    break;
                case FREE_IMAGE_FORMAT::FIF_JPEG:
                    filename += ".jpg";
                    break;
                case FREE_IMAGE_FORMAT::FIF_GIF:
                    filename += ".gif";
                    break;
                default:
                    throw std::runtime_error ("Unsupported free image extension");
            }

            // create a copy of the file information for the filesystem module of irrlicht
            // this will be freed automatically by irrlicht so we can freely delete the container
            // data from memory without leaking memory
            char* filebuffer = new char [mipmap->uncompressedSize];

            memcpy (filebuffer, mipmap->uncompressedData, mipmap->uncompressedSize);

            file = this->m_context->getDevice ()->getFileSystem ()->createMemoryReadFile (
                    filebuffer, mipmap->uncompressedSize, filename.c_str (), true
            );

            if (file == nullptr)
            {
                delete [] filebuffer;
                delete header;

                throw std::runtime_error ("cannot create temporal memory file");
            }

            image = this->m_context->getDevice ()->getVideoDriver ()->createImageFromFile (file);

            if (image == nullptr)
            {
                // this takes care of freeing filebuffer
                file->drop ();

                delete header;

                throw std::runtime_error ("cannot create destination image");
            }
        }

        // delete container info as it's not needed anymore
        delete header;

        return image;
    }

    // load in the image data
    irr::video::IImage *CImageLoaderTex::loadImage (irr::io::IReadFile *input) const
    {
        try
        {
            return this->parseFile (input);
        }
        catch (std::runtime_error ex)
        {
            this->m_context->getDevice ()->getLogger ()->log (
                ex.what (), input->getFileName ().c_str (), irr::ELL_ERROR
            );

            return nullptr;
        }
    }

    void CImageLoaderTex::loadImageFromARGB8Data (irr::video::IImage* output, const char* input, irr::u32 width, irr::u32 height, irr::u32 mipmap_width) const
    {
        irr::u32 bytesPerPixel = output->getBytesPerPixel ();
        char *imagedata = (char *) output->lock ();

        for (irr::u32 y = 0; y < height; y ++)
        {
            irr::u32 baseDestination = y * output->getPitch ();
            irr::u32 baseOrigin = y * (mipmap_width * 4);

            for (irr::u32 x = 0; x < width; x ++)
            {
                imagedata [baseDestination + (x * bytesPerPixel) + 2] = input [baseOrigin + ((width - x) * 4) + 0]; // r
                imagedata [baseDestination + (x * bytesPerPixel) + 1] = input [baseOrigin + ((width - x) * 4) + 1]; // g
                imagedata [baseDestination + (x * bytesPerPixel) + 0] = input [baseOrigin + ((width - x) * 4) + 2]; // b
                imagedata [baseDestination + (x * bytesPerPixel) + 3] = input [baseOrigin + ((width - x) * 4) + 3]; // alpha
            }
        }

        output->unlock ();
    }

    void CImageLoaderTex::loadImageFromDXT1 (irr::video::IImage* output, const char* input, irr::u32 destination_width, irr::u32 destination_height, irr::u32 origin_width, irr::u32 origin_height) const
    {
        char* decompressedBuffer = new char [origin_width * origin_height * 4];

        this->BlockDecompressImageDXT1 (origin_width, origin_height, (const unsigned char*) input, (unsigned long*) decompressedBuffer);
        this->loadImageFromARGB8Data (output, decompressedBuffer, destination_width, destination_height, origin_width);

        delete [] decompressedBuffer;
    }

    void CImageLoaderTex::loadImageFromDXT5 (irr::video::IImage* output, const char* input, irr::u32 destination_width, irr::u32 destination_height, irr::u32 origin_width, irr::u32 origin_height) const
    {
        char* decompressedBuffer = new char [origin_width * origin_height * 4];

        this->BlockDecompressImageDXT5 (origin_width, origin_height, (const unsigned char*) input, (unsigned long*) decompressedBuffer);
        this->loadImageFromARGB8Data (output, decompressedBuffer, destination_width, destination_height, origin_width);

        delete [] decompressedBuffer;
    }

    // ------------------------------------------------------------------------------------
    // The following code is a slightly modified version of this repository
    // https://github.com/Benjamin-Dobell/s3tc-dxt-decompression
    // ------------------------------------------------------------------------------------

    // unsigned long PackRGBA(): Helper method that packs RGBA channels into a single 4 byte pixel.
    //
    // unsigned char r:		red channel.
    // unsigned char g:		green channel.
    // unsigned char b:		blue channel.
    // unsigned char a:		alpha channel.

    unsigned long CImageLoaderTex::PackRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) const
    {
        return ((r << 24) | (g << 16) | (b << 8) | a);
    }

    // void DecompressBlockDXT1(): Decompresses one block of a DXT1 texture and stores the resulting pixels at the appropriate offset in 'image'.
    //
    // unsigned long x:						x-coordinate of the first pixel in the block.
    // unsigned long y:						y-coordinate of the first pixel in the block.
    // unsigned long width: 				width of the texture being decompressed.
    // unsigned long height:				height of the texture being decompressed.
    // const unsigned char *blockStorage:	pointer to the block to decompress.
    // unsigned long *image:				pointer to image where the decompressed pixel data should be stored.

    void CImageLoaderTex::DecompressBlockDXT1(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const
    {
        unsigned short color0 = *reinterpret_cast<const unsigned short *>(blockStorage);
        unsigned short color1 = *reinterpret_cast<const unsigned short *>(blockStorage + 2);

        unsigned long temp;

        temp = (color0 >> 11) * 255 + 16;
        unsigned char r0 = (unsigned char)((temp/32 + temp)/32);
        temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
        unsigned char g0 = (unsigned char)((temp/64 + temp)/64);
        temp = (color0 & 0x001F) * 255 + 16;
        unsigned char b0 = (unsigned char)((temp/32 + temp)/32);

        temp = (color1 >> 11) * 255 + 16;
        unsigned char r1 = (unsigned char)((temp/32 + temp)/32);
        temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
        unsigned char g1 = (unsigned char)((temp/64 + temp)/64);
        temp = (color1 & 0x001F) * 255 + 16;
        unsigned char b1 = (unsigned char)((temp/32 + temp)/32);

        unsigned long code = *reinterpret_cast<const unsigned long *>(blockStorage + 4);

        for (int j=0; j < 4; j++)
        {
            for (int i=0; i < 4; i++)
            {
                unsigned long finalColor = 0;
                unsigned char positionCode = (code >>  2*(4*j+i)) & 0x03;

                if (color0 > color1)
                {
                    switch (positionCode)
                    {
                        case 0:
                            finalColor = PackRGBA(r0, g0, b0, 255);
                            break;
                        case 1:
                            finalColor = PackRGBA(r1, g1, b1, 255);
                            break;
                        case 2:
                            finalColor = PackRGBA((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, 255);
                            break;
                        case 3:
                            finalColor = PackRGBA((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, 255);
                            break;
                    }
                }
                else
                {
                    switch (positionCode)
                    {
                        case 0:
                            finalColor = PackRGBA(r0, g0, b0, 255);
                            break;
                        case 1:
                            finalColor = PackRGBA(r1, g1, b1, 255);
                            break;
                        case 2:
                            finalColor = PackRGBA((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, 255);
                            break;
                        case 3:
                            finalColor = PackRGBA(0, 0, 0, 255);
                            break;
                    }
                }

                if (x + i < width)
                    image[(y + j)*width + (x + i)] = finalColor;
            }
        }
    }

    // void BlockDecompressImageDXT1(): Decompresses all the blocks of a DXT1 compressed texture and stores the resulting pixels in 'image'.
    //
    // unsigned long width:					Texture width.
    // unsigned long height:				Texture height.
    // const unsigned char *blockStorage:	pointer to compressed DXT1 blocks.
    // unsigned long *image:				pointer to the image where the decompressed pixels will be stored.

    void CImageLoaderTex::BlockDecompressImageDXT1(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const
    {
        unsigned long blockCountX = (width + 3) / 4;
        unsigned long blockCountY = (height + 3) / 4;
        unsigned long blockWidth = (width < 4) ? width : 4;
        unsigned long blockHeight = (height < 4) ? height : 4;

        for (unsigned long j = 0; j < blockCountY; j++)
        {
            for (unsigned long i = 0; i < blockCountX; i++) DecompressBlockDXT1(i*4, j*4, width, blockStorage + i * 8, image);
            blockStorage += blockCountX * 8;
        }
    }

    // void DecompressBlockDXT5(): Decompresses one block of a DXT5 texture and stores the resulting pixels at the appropriate offset in 'image'.
    //
    // unsigned long x:						x-coordinate of the first pixel in the block.
    // unsigned long y:						y-coordinate of the first pixel in the block.
    // unsigned long width: 				width of the texture being decompressed.
    // unsigned long height:				height of the texture being decompressed.
    // const unsigned char *blockStorage:	pointer to the block to decompress.
    // unsigned long *image:				pointer to image where the decompressed pixel data should be stored.

    void CImageLoaderTex::DecompressBlockDXT5(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const
    {
        unsigned char alpha0 = *reinterpret_cast<const unsigned char *>(blockStorage);
        unsigned char alpha1 = *reinterpret_cast<const unsigned char *>(blockStorage + 1);

        const unsigned char *bits = blockStorage + 2;
        unsigned long alphaCode1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
        unsigned short alphaCode2 = bits[0] | (bits[1] << 8);

        unsigned short color0 = *reinterpret_cast<const unsigned short *>(blockStorage + 8);
        unsigned short color1 = *reinterpret_cast<const unsigned short *>(blockStorage + 10);

        unsigned long temp;

        temp = (color0 >> 11) * 255 + 16;
        unsigned char r0 = (unsigned char)((temp/32 + temp)/32);
        temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
        unsigned char g0 = (unsigned char)((temp/64 + temp)/64);
        temp = (color0 & 0x001F) * 255 + 16;
        unsigned char b0 = (unsigned char)((temp/32 + temp)/32);

        temp = (color1 >> 11) * 255 + 16;
        unsigned char r1 = (unsigned char)((temp/32 + temp)/32);
        temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
        unsigned char g1 = (unsigned char)((temp/64 + temp)/64);
        temp = (color1 & 0x001F) * 255 + 16;
        unsigned char b1 = (unsigned char)((temp/32 + temp)/32);

        unsigned long code = *reinterpret_cast<const unsigned long *>(blockStorage + 12);

        for (int j=0; j < 4; j++)
        {
            for (int i=0; i < 4; i++)
            {
                int alphaCodeIndex = 3*(4*j+i);
                int alphaCode;

                if (alphaCodeIndex <= 12)
                {
                    alphaCode = (alphaCode2 >> alphaCodeIndex) & 0x07;
                }
                else if (alphaCodeIndex == 15)
                {
                    alphaCode = (alphaCode2 >> 15) | ((alphaCode1 << 1) & 0x06);
                }
                else // alphaCodeIndex >= 18 && alphaCodeIndex <= 45
                {
                    alphaCode = (alphaCode1 >> (alphaCodeIndex - 16)) & 0x07;
                }

                unsigned char finalAlpha;
                if (alphaCode == 0)
                {
                    finalAlpha = alpha0;
                }
                else if (alphaCode == 1)
                {
                    finalAlpha = alpha1;
                }
                else
                {
                    if (alpha0 > alpha1)
                    {
                        finalAlpha = ((8-alphaCode)*alpha0 + (alphaCode-1)*alpha1)/7;
                    }
                    else
                    {
                        if (alphaCode == 6)
                            finalAlpha = 0;
                        else if (alphaCode == 7)
                            finalAlpha = 255;
                        else
                            finalAlpha = ((6-alphaCode)*alpha0 + (alphaCode-1)*alpha1)/5;
                    }
                }

                unsigned char colorCode = (code >> 2*(4*j+i)) & 0x03;

                unsigned long finalColor;
                switch (colorCode)
                {
                    case 0:
                        finalColor = PackRGBA(r0, g0, b0, finalAlpha);
                        break;
                    case 1:
                        finalColor = PackRGBA(r1, g1, b1, finalAlpha);
                        break;
                    case 2:
                        finalColor = PackRGBA((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, finalAlpha);
                        break;
                    case 3:
                        finalColor = PackRGBA((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, finalAlpha);
                        break;
                }

                if (x + i < width)
                    image[(y + j)*width + (x + i)] = finalColor;
            }
        }
    }

    // void BlockDecompressImageDXT5(): Decompresses all the blocks of a DXT5 compressed texture and stores the resulting pixels in 'image'.
    //
    // unsigned long width:					Texture width.
    // unsigned long height:				Texture height.
    // const unsigned char *blockStorage:	pointer to compressed DXT5 blocks.
    // unsigned long *image:				pointer to the image where the decompressed pixels will be stored.

    void CImageLoaderTex::BlockDecompressImageDXT5(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const
    {
        unsigned long blockCountX = (width + 3) / 4;
        unsigned long blockCountY = (height + 3) / 4;
        unsigned long blockWidth = (width < 4) ? width : 4;
        unsigned long blockHeight = (height < 4) ? height : 4;

        for (unsigned long j = 0; j < blockCountY; j++)
        {
            for (unsigned long i = 0; i < blockCountX; i++) DecompressBlockDXT5(i*4, j*4, width, blockStorage + i * 16, image);
            blockStorage += blockCountX * 16;
        }
    }
};