#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Types.h"

namespace WallpaperEngine::Data::Assets {
enum ContainerVersion {
    ContainerVersion_UNKNOWN = 0,
    ContainerVersion_TEXB0001 = 1,
    ContainerVersion_TEXB0002 = 2,
    ContainerVersion_TEXB0003 = 3,
    ContainerVersion_TEXB0004 = 4,
};

enum AnimatedVersion {
    AnimatedVersion_UNKNOWN = 0,
    AnimatedVersion_TEXS0002 = 2,
    AnimatedVersion_TEXS0003 = 3,
};

enum FIF {
    FIF_UNKNOWN = -1,
    FIF_BMP = 0,
    FIF_ICO = 1,
    FIF_JPEG = 2,
    FIF_JNG = 3,
    FIF_KOALA = 4,
    FIF_LBM = 5,
    FIF_IFF = FIF_LBM,
    FIF_MNG = 6,
    FIF_PBM = 7,
    FIF_PBMRAW = 8,
    FIF_PCD = 9,
    FIF_PCX = 10,
    FIF_PGM = 11,
    FIF_PGMRAW = 12,
    FIF_PNG = 13,
    FIF_PPM = 14,
    FIF_PPMRAW = 15,
    FIF_RAS = 16,
    FIF_TARGA = 17,
    FIF_TIFF = 18,
    FIF_WBMP = 19,
    FIF_PSD = 20,
    FIF_CUT = 21,
    FIF_XBM = 22,
    FIF_XPM = 23,
    FIF_DDS = 24,
    FIF_GIF = 25,
    FIF_HDR = 26,
    FIF_FAXG3 = 27,
    FIF_SGI = 28,
    FIF_EXR = 29,
    FIF_J2K = 30,
    FIF_JP2 = 31,
    FIF_PFM = 32,
    FIF_PICT = 33,
    FIF_RAW = 34,
    FIF_WEBP = 35,
    FIF_MP4 = FIF_WEBP,
    FIF_JXR = 36
};

enum TextureFormat {
    TextureFormat_UNKNOWN = 0xFFFFFFFF,
    TextureFormat_ARGB8888 = 0,
    TextureFormat_RGB888 = 1,
    TextureFormat_RGB565 = 2,
    TextureFormat_DXT5 = 4,
    TextureFormat_DXT3 = 6,
    TextureFormat_DXT1 = 7,
    TextureFormat_RG88 = 8,
    TextureFormat_R8 = 9,
    TextureFormat_RG1616f = 10,
    TextureFormat_R16f = 11,
    TextureFormat_BC7 = 12,
    TextureFormat_RGBa1010102 = 13,
    TextureFormat_RGBA16161616f = 14,
    TextureFormat_RGB161616f = 15,
};

enum TextureFlags {
    TextureFlags_NoFlags = 0,
    TextureFlags_NoInterpolation = 1,
    TextureFlags_ClampUVs = 2,
    TextureFlags_IsGif = 4,
    TextureFlags_ClampUVsBorder = 8,
    TextureFlags_AlphaChannelPriority = 524288, // Indicates RG88/R8 format where alpha is in G/R channel
    TextureFlags_All = TextureFlags_NoInterpolation | TextureFlags_ClampUVs | TextureFlags_IsGif
	| TextureFlags_ClampUVsBorder | TextureFlags_AlphaChannelPriority,
};

struct Mipmap {
    /** Width of the mipmap */
    uint32_t width = 0;
    /** Height of the mipmap */
    uint32_t height = 0;
    /** If the mipmap data is compressed */
    uint32_t compression = 0;
    /** Uncompressed size of the mipmap */
    int uncompressedSize = 0;
    /** Compress size of the mipmap */
    int compressedSize = 0;
    /** Pointer to the compressed data */
    std::unique_ptr<char[]> compressedData = nullptr;
    /** Pointer to the uncompressed data */
    std::unique_ptr<char[]> uncompressedData = nullptr;
    /** JSON data */
    std::string json {};
};

struct Frame {
    /** The image index of this frame */
    uint32_t frameNumber = 0;
    /** The amount of time this frame spends being displayed */
    float frametime = 0.0f;
    /** The x position of the frame in the texture */
    float x = 0.0f;
    /** The y position of the frame in the texture */
    float y = 0.0f;
    /** The width of the frame in the texture */
    float width1 = 0.0f;
    float width2 = 0.0f;
    /** The height of the frame in the texture */
    float height1 = 0.0f;
    float height2 = 0.0f;
};

struct Texture {
    /** The version of the texture container */
    ContainerVersion containerVersion = ContainerVersion_UNKNOWN;
    /** The version of the animated data */
    AnimatedVersion animatedVersion = AnimatedVersion_UNKNOWN;
    /** Flags with extra texture information @see TextureFlags */
    uint32_t flags = TextureFlags_NoFlags;
    /** Real width of the texture */
    uint32_t width = 0;
    /** Real height of the texture */
    uint32_t height = 0;
    /** Texture width in memory (power of 2) */
    uint32_t textureWidth = 0;
    /** Texture height in memory (power of 2) */
    uint32_t textureHeight = 0;
    /** Gif width */
    uint32_t gifWidth = 0;
    /** Gif height */
    uint32_t gifHeight = 0;
    /** Texture data format */
    TextureFormat format = TextureFormat_UNKNOWN;
    /** Free Image format */
    FIF freeImageFormat = FIF_UNKNOWN;
    /** Indicates if we have an MP4 video */
    bool isVideoMp4 = false;
    /** The amount of images in the texture file */
    uint32_t imageCount = 0;
    /** List of mipmaps */
    std::map<uint32_t, MipmapList> images {};
    /** List of animation frames */
    std::vector<FrameSharedPtr> frames {};

    /** Spritesheet grid data (from .tex-json metadata) */
    uint32_t spritesheetCols = 0;
    uint32_t spritesheetRows = 0;
    uint32_t spritesheetFrames = 0;
    float spritesheetDuration = 0.0f;

    [[nodiscard]] bool isAnimated () const { return (flags & TextureFlags_IsGif) == TextureFlags_IsGif; }

    [[nodiscard]] bool hasSpritesheet () const { return spritesheetFrames > 0; }
};
}