#include <cstring>

#include <lz4.h>

#include "TextureParser.h"
#include "WallpaperEngine/Data/Assets/Texture.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Parsers;

TextureUniquePtr TextureParser::parse (const BinaryReader& file) {
    auto result = std::make_unique<Texture> ();

    parseTextureHeader (*result, file);
    parseContainer (*result, file);

    for (uint32_t image = 0; image < result->imageCount; image++) {
        const uint32_t mipmapCount = file.nextUInt32 ();
        MipmapList mipmaps;

        for (uint32_t mipmap = 0; mipmap < mipmapCount; mipmap++) {
            mipmaps.emplace_back (parseMipmap (file, *result));
        }

        result->images.emplace (image, mipmaps);
    }

    if (!result->isAnimated ()) {
        return result;
    }

    parseAnimations (*result, file);

    return result;
}

MipmapSharedPtr TextureParser::parseMipmap (const BinaryReader& file, const Texture& header) {
    auto result = std::make_shared<Mipmap> ();

    // TEXB0004 has some extra data in the header that has to be handled
    if (header.containerVersion == ContainerVersion_TEXB0004) {
        // some integers that we can ignore as they only seem to affect
        // the editor
        std::ignore = file.nextUInt32 ();
        std::ignore = file.nextUInt32 ();
        // this format includes some json in the header that we might need
        // to parse at some point...
        result->json = file.nextNullTerminatedString ();
        // last ignorable integer
        std::ignore = file.nextUInt32 ();
    }

    result->width = file.nextUInt32 ();
    result->height = file.nextUInt32 ();

    if (header.containerVersion == ContainerVersion_TEXB0004 ||
        header.containerVersion == ContainerVersion_TEXB0003 ||
        header.containerVersion == ContainerVersion_TEXB0002) {
        result->compression = file.nextUInt32 ();
        result->uncompressedSize = file.nextInt ();
    }

    result->compressedSize = file.nextInt ();

    if (result->compression == 0) {
        // this might be better named as mipmap_bytes_size instead of compressedSize
        // as in uncompressed files this variable actually holds the file length
        result->uncompressedSize = result->compressedSize;
    }

    result->uncompressedData = std::unique_ptr<char[]> (new char [result->uncompressedSize]);

    if (result->compression == 1) {
        result->compressedData = std::unique_ptr<char[]> (new char [result->compressedSize]);
        // read the compressed data into the buffer
        file.next (result->compressedData.get (), result->compressedSize);
        // finally decompress it
        int bytes = LZ4_decompress_safe (
            result->compressedData.get (), result->uncompressedData.get (), result->compressedSize,
            result->uncompressedSize
        );

        if (bytes < 0)
            sLog.exception ("Cannot decompress texture data, LZ4_decompress_safe returned an error");
    } else {
        file.next (result->uncompressedData.get (), result->uncompressedSize);
    }

    return result;
}

FrameSharedPtr TextureParser::parseFrame (const BinaryReader& file) {
    auto result = std::make_shared<Frame> ();

    result->frameNumber = file.nextUInt32 ();
    result->frametime = file.nextFloat ();
    result->x = file.nextFloat ();
    result->y = file.nextFloat ();
    result->width1 = file.nextFloat ();
    result->width2 = file.nextFloat ();
    result->height2 = file.nextFloat ();
    result->height1 = file.nextFloat ();

    return result;
}

TextureFormat TextureParser::parseTextureFormat (uint32_t value) {
    switch (value) {
        case TextureFormat_UNKNOWN:
        case TextureFormat_ARGB8888:
        case TextureFormat_RGB888:
        case TextureFormat_RGB565:
        case TextureFormat_DXT5:
        case TextureFormat_DXT3:
        case TextureFormat_DXT1:
        case TextureFormat_RG88:
        case TextureFormat_R8:
        case TextureFormat_RG1616f:
        case TextureFormat_R16f:
        case TextureFormat_BC7:
        case TextureFormat_RGBa1010102:
        case TextureFormat_RGBA16161616f:
        case TextureFormat_RGB161616f:
            return static_cast<TextureFormat> (value);

        default:
            sLog.exception ("unknown texture format: ", value);
    }
}

void TextureParser::parseTextureHeader (Texture& header, const BinaryReader& file) {
    char magic[9] = { 0 };

    file.next (magic, 9);

    if (strncmp (magic, "TEXV0005", 9) != 0)
        sLog.exception ("unexpected texture container type: ", std::string_view (magic, 9));

    file.next (magic, 9);

    if (strncmp (magic, "TEXI0001", 9) != 0)
        sLog.exception ("unexpected texture sub-container type: ", std::string_view (magic, 9));


    header.format = parseTextureFormat (file.nextUInt32 ());
    header.flags = parseTextureFlags (file.nextUInt32 ());
    header.textureWidth = file.nextUInt32 ();
    header.textureHeight = file.nextUInt32 ();
    header.width = file.nextUInt32 ();
    header.height = file.nextUInt32 ();

    // ignore some more bytes
    std::ignore = file.nextUInt32 ();
}

void TextureParser::parseContainer (Texture& header, const BinaryReader& file) {
    char magic[9] = { 0 };

    file.next (magic, 9);

    header.imageCount = file.nextUInt32 ();

    if (strncmp (magic, "TEXB0004", 9) == 0) {
        header.containerVersion = ContainerVersion_TEXB0004;
        header.freeImageFormat = parseFIF (file.nextUInt32 ());
        header.isVideoMp4 = file.nextUInt32 () == 1;

        if (header.freeImageFormat == FIF_UNKNOWN && header.isVideoMp4) {
            header.freeImageFormat = FIF_MP4;
        }

        // default to TEXB0003 format here
        if (header.freeImageFormat != FIF_MP4) {
            header.containerVersion = ContainerVersion_TEXB0003;
        }
    } else if (strncmp (magic, "TEXB0003", 9) == 0) {
        header.containerVersion = ContainerVersion_TEXB0003;
        header.freeImageFormat = parseFIF (file.nextUInt32 ());
    } else if (strncmp (magic, "TEXB0002", 9) == 0) {
        header.containerVersion = ContainerVersion_TEXB0002;
    } else if (strncmp (magic, "TEXB0001", 9) == 0) {
        header.containerVersion = ContainerVersion_TEXB0001;
    } else {
        sLog.exception ("unknown texture format type: ", std::string_view (magic, 9));
    }
}

void TextureParser::parseAnimations (Texture& header, const BinaryReader& file) {
    char magic[9] = { 0 };

    // image is animated, keep parsing the rest of the image info
    file.next (magic, 9);

    if (strncmp (magic, "TEXS0002", 9) == 0) {
        header.animatedVersion = AnimatedVersion_TEXS0002;
    } else if (strncmp (magic, "TEXS0003", 9) == 0) {
        header.animatedVersion = AnimatedVersion_TEXS0003;
    } else {
        sLog.exception ("found animation information of unknown type: ", std::string_view (magic, 9));
    }

    uint32_t frameCount = file.nextUInt32 ();

    if (header.animatedVersion == AnimatedVersion_TEXS0003) {
        header.gifWidth = file.nextUInt32 ();
        header.gifHeight = file.nextUInt32 ();
    }

    while (frameCount-- > 0) {
        header.frames.push_back (parseFrame (file));
    }

    // ensure gif width and height is right for TEXS0002
    if (header.animatedVersion == AnimatedVersion_TEXS0002) {
        header.gifWidth = (*header.frames.begin ())->width1;
        header.gifHeight = (*header.frames.begin ())->height1;
    }
}

uint32_t TextureParser::parseTextureFlags (uint32_t value) {
    if (value < TextureFlags_All) {
        return value;
    }

    sLog.exception ("unknown texture flags: ", value);
}

FIF TextureParser::parseFIF (uint32_t value) {
    switch (value) {
        case FIF_UNKNOWN:
        case FIF_BMP:
        case FIF_ICO:
        case FIF_JPEG:
        case FIF_JNG:
        case FIF_KOALA:
        case FIF_LBM:
        case FIF_MNG:
        case FIF_PBM:
        case FIF_PBMRAW:
        case FIF_PCD:
        case FIF_PCX:
        case FIF_PGM:
        case FIF_PGMRAW:
        case FIF_PNG:
        case FIF_PPM:
        case FIF_PPMRAW:
        case FIF_RAS:
        case FIF_TARGA:
        case FIF_TIFF:
        case FIF_WBMP:
        case FIF_PSD:
        case FIF_CUT:
        case FIF_XBM:
        case FIF_XPM:
        case FIF_DDS:
        case FIF_GIF:
        case FIF_HDR:
        case FIF_FAXG3:
        case FIF_SGI:
        case FIF_EXR:
        case FIF_J2K:
        case FIF_JP2:
        case FIF_PFM:
        case FIF_PICT:
        case FIF_RAW:
        case FIF_WEBP:
        case FIF_JXR:
            return static_cast<FIF> (value);

        default:
            sLog.exception ("unknown free image format: ", value);
    }
}