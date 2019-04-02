// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CImageLoaderTEX.h"

#include <irrlicht/irrlicht.h>
#include <lz4.h>
#include <wallpaperengine/irrlicht.h>
#include <string>

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
                return 0;

            video::IImage *image = 0;

            char buffer [1024];

            if (input->read (buffer, 9) != 9)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot read header\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
            }

            if (memcmp (buffer, "TEXV0005", 9) != 0)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: not really a tex\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
            }

            if (input->read (buffer, 9) != 9)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot read second header\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
            }

            if (memcmp (buffer, "TEXI0001", 9) != 0)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: not really a tex\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
            }

            u32 width;
            u32 height;
            u32 texture_width;
            u32 texture_height;
            u32 format;

            input->read (&format, 4);
            input->seek (4, true); // ignore bytes
            input->read (&texture_width, 4);
            input->read (&texture_height, 4);
            input->read (&width, 4);
            input->read (&height, 4);
            input->seek (4, true); // ignore bytes
            input->read (buffer, 9);

            if (memcmp (buffer, "TEXB0003", 9) == 0)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: TEXB0003 container not supported yet\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
            }
            else if (memcmp (buffer, "TEXB0002", 9) == 0)
            {
                input->seek (4, true);
            }
            else
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: Unknown container type\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
            }

            if (format != TextureFormat::ARGB8888)
            {
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: Only ARGB8888 supported\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
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
            input->read (&mipmap_compression, 4);
            input->read (&mipmap_uncompressed_size, 4);
            input->read (&mipmap_compressed_size, 4);

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
                    return 0;
                }

                delete [] compressedBuffer;
            }
            else
            {
                input->read (decompressedBuffer, mipmap_uncompressed_size);
            }

            image = wp::irrlicht::driver->createImage (ECF_A8R8G8B8, irr::core::dimension2d<u32> (width, height));

            if (!image)
            {
                delete [] decompressedBuffer;
                delete image;
                wp::irrlicht::device->getLogger ()->log ("LOAD TEX: cannot create destination image\n", input->getFileName ().c_str (), irr::ELL_ERROR);
                return 0;
            }

            u32 bytesPerPixel = image->getBytesPerPixel ();
            char *imagedata = (char *) image->lock ();

            for (u32 y = 0; y < height; y ++)
            {
                u32 baseDestination = y * image->getPitch ();
                u32 baseOrigin = y * (mipmap_width * 4);

                for (u32 x = 0; x < width; x ++)
                {
                    imagedata [baseDestination + (x * bytesPerPixel) + 2] = decompressedBuffer [baseOrigin + ((width - x) * 4) + 0]; // r
                    imagedata [baseDestination + (x * bytesPerPixel) + 1] = decompressedBuffer [baseOrigin + ((width - x) * 4) + 1]; // g
                    imagedata [baseDestination + (x * bytesPerPixel) + 0] = decompressedBuffer [baseOrigin + ((width - x) * 4) + 2]; // b
                    imagedata [baseDestination + (x * bytesPerPixel) + 3] = decompressedBuffer [baseOrigin + ((width - x) * 4) + 3]; // alpha
                }
            }

            image->unlock ();

            delete [] decompressedBuffer;

            return image;
        }


    }// end namespace irr
}//end namespace video

