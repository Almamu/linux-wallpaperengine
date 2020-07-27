
#include "CImageLoaderTEX.h"

#include <irrlicht/irrlicht.h>
#include <lz4.h>
#include <wallpaperengine/irrlicht.h>
#include <string>
#include <algorithm>

namespace irr {
    namespace video {

        //! returns true if the file maybe is able to be loaded by this class
        //! based on the file extension (e.g. ".tga")
        bool CImageLoaderTex::isALoadableFileExtension (const io::path &filename) const
        {
            return core::hasFileExtension (filename, "tex");
        }


        //! returns true if the file maybe is able to be loaded by this class
        bool CImageLoaderTex::isALoadableFileFormat (io::IReadFile *file) const
        {
            return false;
        }


        // load in the image data
        IImage *CImageLoaderTex::loadImage (io::IReadFile *input) const
        {
            if (!input)
                return nullptr;

            video::IImage *image = nullptr;

            char buffer [1024];

            if (input->read (buffer, 9) != 9)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot read header\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return nullptr;
            }

            if (memcmp (buffer, "TEXV0005", 9) != 0)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: not really a tex\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return nullptr;
            }

            if (input->read (buffer, 9) != 9)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot read second header\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return nullptr;
            }

            if (memcmp (buffer, "TEXI0001", 9) != 0)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: not really a tex\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return nullptr;
            }

            u32 width;
            u32 height;
            u32 texture_width;
            u32 texture_height;
            u32 format;
            u32 imageFormat = FREE_IMAGE_FORMAT::FIF_UNKNOWN;
            u8 containerVersion = 0;

            input->read (&format, 4);
            input->seek (4, true); // ignore bytes
            input->read (&texture_width, 4);
            input->read (&texture_height, 4);
            input->read (&width, 4);
            input->read (&height, 4);
            input->seek (4, true); // ignore bytes
            input->read (buffer, 9);

            if (input->getFileName ().find ("materials/flowmask.tex") != std::string::npos ||
                input->getFileName ().find ("materials/masks/godrays_downsample2_mask") != std::string::npos ||
                input->getFileName ().find ("materials/util/white.tex") != std::string::npos)
            {
                // relevant shaders are currently drawing these masks opaque; return a transparent image instead
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: Skipping broken mask", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                return wp::irrlicht::driver->createImage (ECF_A8R8G8B8, irr::core::dimension2d<u32> (width, height));
            }
            if (memcmp (buffer, "TEXB0003", 9) == 0)
            {
                containerVersion = 3;
                input->seek (4, true);
                input->read (&imageFormat, 4);
            }
            else if (memcmp (buffer, "TEXB0002", 9) == 0)
            {
                containerVersion = 2;

                input->seek (4, true);
            }
            else if (memcmp (buffer, "TEXB0001", 9) == 0)
            {
                containerVersion = 1;
                input->seek (4, true);
            }
            else
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: Unknown container type\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return nullptr;
            }

            if (format == TextureFormat::A8)
            {
                wp::irrlicht::device->getLogger ()-> log ("LOAD TEX: A8 not supported\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return nullptr;
            }

            if (format == TextureFormat::RA88)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: RA88 not supported\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return nullptr;
            }

            u32 mipmap_count = 0;

            input->read (&mipmap_count, 4);

            u32 mipmap_width = 0;
            u32 mipmap_height = 0;
            u32 mipmap_compression = 0;
            u32 mipmap_uncompressed_size = 0;
            u32 mipmap_compressed_size = 0;

            input->read (&mipmap_width, 4);
            input->read (&mipmap_height, 4);

            if (containerVersion > 1)
            {
                input->read (&mipmap_compression, 4);
                input->read (&mipmap_uncompressed_size, 4);
            }

            input->read (&mipmap_compressed_size, 4);

            // TODO: BETTER POSITION FOR THIS
            if (mipmap_compression == 0)
            {
                // this might be better named as mipmap_bytes_size instead of compressed_size
                // as in uncompressed files this variable actually holds the file length
                mipmap_uncompressed_size = mipmap_compressed_size;
            }

            char *decompressedBuffer = new char [mipmap_uncompressed_size];

            if (mipmap_compression == 1)
            {
                char *compressedBuffer = new char [mipmap_compressed_size];

                input->read (compressedBuffer, mipmap_compressed_size);

                int result = LZ4_decompress_safe (compressedBuffer, decompressedBuffer, mipmap_compressed_size, mipmap_uncompressed_size);

                if (!result)
                {
                    delete [] decompressedBuffer;
                    delete [] compressedBuffer;
                    wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot decompress texture data\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                    return nullptr;
                }

                delete [] compressedBuffer;
            }
            else
            {
                input->read (decompressedBuffer, mipmap_uncompressed_size);
            }

            if (imageFormat == FREE_IMAGE_FORMAT::FIF_UNKNOWN)
            {
                image = wp::irrlicht::driver->createImage (ECF_A8R8G8B8, irr::core::dimension2d<u32> (width, height));

                if (!image)
                {
                    delete [] decompressedBuffer;
                    delete image;
                    wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot create destination image\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                    return nullptr;
                }

                switch (format)
                {
                    case TextureFormat::ARGB8888:
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is an ARGB8", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        this->loadImageFromARGB8Data (image, decompressedBuffer, width, height, mipmap_width);
                        break;
                    case TextureFormat::DXT1:
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is a DXT1", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        this->loadImageFromDXT1 (image, decompressedBuffer, width, height, mipmap_width, mipmap_height);
                        break;
                    case TextureFormat::DXT3:
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is a DXT3", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        this->loadImageFromDXT3 (image, decompressedBuffer, width, height, mipmap_width, mipmap_height);
                        break;
                    case TextureFormat::DXT5:
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is a DXT5", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        this->loadImageFromDXT5 (image, decompressedBuffer, width, height, mipmap_width, mipmap_height);
                        break;
                    default:
                        delete [] decompressedBuffer;
                        delete image;
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: Unknown texture format\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                        return nullptr;
                }
            }
            else
            {
                // copy the buffer to a new address
                char* filebuffer = new char [mipmap_uncompressed_size];
                char tmpname [TMP_MAX];

                // copy file data to the final file buffer to be used
                memcpy (filebuffer, decompressedBuffer, mipmap_uncompressed_size);
                // generate temporal name
                mkstemp (tmpname);
                // store it in a std::string
                std::string filename = tmpname;
                irr::io::IReadFile* file;

                // free image format
                switch (imageFormat)
                {
                    case FREE_IMAGE_FORMAT::FIF_BMP:
                        // add extension to the file
                        filename += ".bmp";
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is a BMP", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        file = wp::irrlicht::device->getFileSystem ()->createMemoryReadFile (filebuffer, mipmap_uncompressed_size, filename.c_str (), true);
                        break;
                    case FREE_IMAGE_FORMAT::FIF_PNG:
                        // add extension to the file
                        filename += ".png";
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is a PNG", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        file = wp::irrlicht::device->getFileSystem ()->createMemoryReadFile (filebuffer, mipmap_uncompressed_size, filename.c_str (), true);
                        break;
                    case FREE_IMAGE_FORMAT::FIF_JPEG:
                        // add extension to the file
                        filename += ".jpg";
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is a JPG", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        //wp::irrlicht::device->getFileSystem ()->createAndWriteFile ("/tmp/test.jpg", false)->write (filebuffer, mipmap_uncompressed_size);
                        file = wp::irrlicht::device->getFileSystem ()->createMemoryReadFile (filebuffer, mipmap_uncompressed_size, filename.c_str (), true);
                        break;
                    case FREE_IMAGE_FORMAT::FIF_GIF:
                        filename += ".gif";
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: This is a GIF", input->getFileName ().c_str (), irr::ELL_INFORMATION);
                        file = wp::irrlicht::device->getFileSystem ()->createMemoryReadFile (filebuffer, mipmap_uncompressed_size, filename.c_str (), true);
                        break;
                    default:
                        wp::irrlicht::device->getLogger ()->log ("LOAD TEX: detected unsupported free-image format\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                        delete [] decompressedBuffer;
                        delete [] filebuffer;
                        return nullptr;
                }

                image = wp::irrlicht::driver->createImageFromFile (file);

                if (!image)
                {
                    file->drop ();
                    
                    delete [] decompressedBuffer;
                    delete image;
                    wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot create destination image\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                    return nullptr;
                }
            }

            delete [] decompressedBuffer;

#if 0
            // dump image to TGA file (adapted from maluoi's gist)
            u32 bytesPerPixel = image->getBytesPerPixel ();
            if (bytesPerPixel != 3 && bytesPerPixel != 4)
                wp::irrlicht::device->getLogger ()->log (("Unexpected bytesPerPixel of " + std::to_string (bytesPerPixel)).c_str (), input->getFileName ().c_str (), irr::ELL_ERROR);
            char hasNoAlpha = (bytesPerPixel < 4);

            std::string fileName = input->getFileName ().c_str ();
            std::replace (fileName.begin (), fileName.end (), '/', '-');
            std::string path = std::string (getenv("HOME")) + "/stuff/wallpaperengine-dumps/";
            system (("mkdir -p " + path).c_str ());
            path += fileName + ".tga";
            FILE *dumpFile = fopen (path.c_str (), "wb");
            uint8_t header[18] = { 0,0,2,0,0,0,0,0,0,0,0,0,
                (uint8_t)(width%256), (uint8_t)(width/256), (uint8_t)(height%256), (uint8_t)(height/256), 32, 32 };
            fwrite (&header, 18, 1, dumpFile);

            char *imagedata = (char *) image->lock ();
            for (u32 y = 0; y < image->getDimension ().Height; y ++)
            {
                u32 baseDestination = y * image->getPitch ();
                for (u32 x = 0; x < width; x ++)
                {
                    static unsigned char color[4];
                    color[0+2*hasNoAlpha]     = imagedata [baseDestination + (x * bytesPerPixel) + 0]; // b
                    color[1]                  = imagedata [baseDestination + (x * bytesPerPixel) + 1]; // g
                    color[2-2*hasNoAlpha]     = imagedata [baseDestination + (x * bytesPerPixel) + 2]; // r
                    color[3] = 255*hasNoAlpha | imagedata [baseDestination + (x * bytesPerPixel) + 3]; // a
                    fwrite (color, 1, 4, dumpFile);
                }
            }
            image->unlock ();
            fclose (dumpFile);
#endif
            return image;
        }

        void CImageLoaderTex::loadImageFromARGB8Data (IImage* output, const char* input, u32 width, u32 height, u32 mipmap_width) const
        {
            u32 bytesPerPixel = output->getBytesPerPixel ();
            char *imagedata = (char *) output->lock ();

            for (u32 y = 0; y < height; y ++)
            {
                u32 baseDestination = y * output->getPitch ();
                u32 baseOrigin = y * (mipmap_width * 4);

                for (u32 x = 0; x < width; x ++)
                {
                    imagedata [baseDestination + (x * bytesPerPixel) + 2] = input [baseOrigin + (x * 4) + 0]; // r
                    imagedata [baseDestination + (x * bytesPerPixel) + 1] = input [baseOrigin + (x * 4) + 1]; // g
                    imagedata [baseDestination + (x * bytesPerPixel) + 0] = input [baseOrigin + (x * 4) + 2]; // b
                    imagedata [baseDestination + (x * bytesPerPixel) + 3] = input [baseOrigin + (x * 4) + 3]; // a
                }
            }

            output->unlock ();
        }

        void CImageLoaderTex::loadImageFromDXT1 (IImage* output, const char* input, u32 destination_width, u32 destination_height, u32 origin_width, u32 origin_height) const
        {
            char* decompressedBuffer = new char [origin_width * origin_height * 4];

            this->BlockDecompressImageDXT1 (origin_width, origin_height, (const unsigned char*) input, (unsigned long*) decompressedBuffer);
            this->loadImageFromARGB8Data (output, decompressedBuffer, destination_width, destination_height, origin_width);

            delete [] decompressedBuffer;
        }

        void CImageLoaderTex::loadImageFromDXT3 (IImage* output, const char* input, u32 destination_width, u32 destination_height, u32 origin_width, u32 origin_height) const
        {
            char* decompressedBuffer = new char [origin_width * origin_height * 4];

            this->BlockDecompressImageDXT3 (origin_width, origin_height, (const unsigned char*) input, (unsigned long*) decompressedBuffer);
            this->loadImageFromARGB8Data (output, decompressedBuffer, destination_width, destination_height, origin_width);

            delete [] decompressedBuffer;
        }

        void CImageLoaderTex::loadImageFromDXT5 (IImage* output, const char* input, u32 destination_width, u32 destination_height, u32 origin_width, u32 origin_height) const
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

        unsigned long CImageLoaderTex::PackRGBA (unsigned char r, unsigned char g, unsigned char b, unsigned char a) const
        {
            return r | g << 8 | b << 16 | a << 24;
        }

        // void DecompressBlockDXT1(): Decompresses one block of a DXT1 texture and stores the resulting pixels at the appropriate offset in 'image'.
        //
        // unsigned long x:						x-coordinate of the first pixel in the block.
        // unsigned long y:						y-coordinate of the first pixel in the block.
        // unsigned long width: 				width of the texture being decompressed.
        // unsigned long height:				height of the texture being decompressed.
        // const unsigned char *blockStorage:	pointer to the block to decompress.
        // unsigned long *image:				pointer to image where the decompressed pixel data should be stored.

        void CImageLoaderTex::DecompressBlockDXT1 (unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const
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
                    unsigned char positionCode = (code >> 2*(4*j+i)) & 0x03;

                    if (color0 > color1)
                    {
                        switch (positionCode)
                        {
                            case 0:
                                finalColor = PackRGBA (r0, g0, b0, 255);
                                break;
                            case 1:
                                finalColor = PackRGBA (r1, g1, b1, 255);
                                break;
                            case 2:
                                finalColor = PackRGBA ((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, 255);
                                break;
                            case 3:
                                finalColor = PackRGBA ((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, 255);
                                break;
                        }
                    }
                    else
                    {
                        switch (positionCode)
                        {
                            case 0:
                                finalColor = PackRGBA (r0, g0, b0, 255);
                                break;
                            case 1:
                                finalColor = PackRGBA (r1, g1, b1, 255);
                                break;
                            case 2:
                                finalColor = PackRGBA ((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, 255);
                                break;
                            case 3:
                                finalColor = PackRGBA (0, 0, 0, 255);
                                break;
                        }
                    }

                    reinterpret_cast<u32 *>(image)[(y + j)*width + x + i] = finalColor;
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
                for (unsigned long i = 0; i < blockCountX; i++)
                    DecompressBlockDXT1 (i*4, j*4, width, blockStorage + i * 8, image);
                blockStorage += blockCountX * 8;
            }
        }

        // void DecompressBlockDXT3(): Decompresses one block of a DXT3 texture and stores the resulting pixels at the appropriate offset in 'image'.
        //
        // unsigned long x:						x-coordinate of the first pixel in the block.
        // unsigned long y:						y-coordinate of the first pixel in the block.
        // unsigned long width: 				width of the texture being decompressed.
        // unsigned long height:				height of the texture being decompressed.
        // const unsigned char *blockStorage:	pointer to the block to decompress.
        // unsigned long *image:				pointer to image where the decompressed pixel data should be stored.

        void CImageLoaderTex::DecompressBlockDXT3(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image) const
        {
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
            unsigned long long alphaCode = *reinterpret_cast<const unsigned long long *>(blockStorage);

            for (int j=0; j < 4; j++)
            {
                for (int i=0; i < 4; i++)
                {
                    unsigned long finalColor = 0;
                    unsigned char positionCode = (code >> 2*(4*j+i)) & 0x03;
                    // per StackOverflow this creates an even distribution of alphas between 0 and 255
                    unsigned char finalAlpha = 17 * (alphaCode >> 4*(4*j+i) & 0x0F);

                    if (color0 > color1)
                    {
                        switch (positionCode)
                        {
                            case 0:
                                finalColor = PackRGBA (r0, g0, b0, finalAlpha);
                                break;
                            case 1:
                                finalColor = PackRGBA (r1, g1, b1, finalAlpha);
                                break;
                            case 2:
                                finalColor = PackRGBA ((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, finalAlpha);
                                break;
                            case 3:
                                finalColor = PackRGBA ((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, finalAlpha);
                                break;
                        }
                    }
                    else
                    {
                        switch (positionCode)
                        {
                            case 0:
                                finalColor = PackRGBA (r0, g0, b0, finalAlpha);
                                break;
                            case 1:
                                finalColor = PackRGBA (r1, g1, b1, finalAlpha);
                                break;
                            case 2:
                                finalColor = PackRGBA ((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, finalAlpha);
                                break;
                            case 3:
                                finalColor = PackRGBA (0, 0, 0, finalAlpha);
                                break;
                        }
                    }

                    reinterpret_cast<u32 *>(image)[(y + j)*width + x + i] = finalColor;
                }
            }
        }

        // void BlockDecompressImageDXT3(): Decompresses all the blocks of a DXT3 compressed texture and stores the resulting pixels in 'image'.
        //
        // unsigned long width:					Texture width.
        // unsigned long height:				Texture height.
        // const unsigned char *blockStorage:	pointer to compressed DXT3 blocks.
        // unsigned long *image:				pointer to the image where the decompressed pixels will be stored.

        void CImageLoaderTex::BlockDecompressImageDXT3(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image) const
        {
            unsigned long blockCountX = (width + 3) / 4;
            unsigned long blockCountY = (height + 3) / 4;
            unsigned long blockWidth = (width < 4) ? width : 4;
            unsigned long blockHeight = (height < 4) ? height : 4;
            for (unsigned long j = 0; j < blockCountY; j++)
            {
                for (unsigned long i = 0; i < blockCountX; i++)
                    DecompressBlockDXT3 (i*4, j*4, width, blockStorage + i * 16, image);
                blockStorage += blockCountX * 16;
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

                    unsigned long finalColor = 0;
                    unsigned char positionCode = (code >> 2*(4*j+i)) & 0x03;

                    if (color0 > color1)
                    {
                        switch (positionCode)
                        {
                            case 0:
                                finalColor = PackRGBA (r0, g0, b0, finalAlpha);
                                break;
                            case 1:
                                finalColor = PackRGBA (r1, g1, b1, finalAlpha);
                                break;
                            case 2:
                                finalColor = PackRGBA ((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, finalAlpha);
                                break;
                            case 3:
                                finalColor = PackRGBA ((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, finalAlpha);
                                break;
                        }
                    }
                    else
                    {
                        switch (positionCode)
                        {
                            case 0:
                                finalColor = PackRGBA (r0, g0, b0, finalAlpha);
                                break;
                            case 1:
                                finalColor = PackRGBA (r1, g1, b1, finalAlpha);
                                break;
                            case 2:
                                finalColor = PackRGBA ((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, finalAlpha);
                                break;
                            case 3:
                                finalColor = PackRGBA (0, 0, 0, finalAlpha);
                                break;
                        }
                    }

                    reinterpret_cast<u32 *>(image)[(y + j)*width + x + i] = finalColor;
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
                for (unsigned long i = 0; i < blockCountX; i++)
                    DecompressBlockDXT5 (i*4, j*4, width, blockStorage + i * 16, image);
                blockStorage += blockCountX * 16;
            }
        }
    }// end namespace irr
}//end namespace video

