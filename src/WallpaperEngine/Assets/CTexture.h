#pragma once

#include "ITexture.h"

#include <GL/glew.h>
#include <glm/vec4.hpp>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace WallpaperEngine::Assets {
/**
 * A normal texture file in WallpaperEngine's format
 */
class CTexture final : public ITexture {
    /**
     * Different texture container versions supported
     */
    enum ContainerVersion : int {
        UNKNOWN = -1,
        TEXB0004 = 4,
        TEXB0003 = 3,
        TEXB0002 = 2,
        TEXB0001 = 1
    };

    /**
     * Different texture animation versions supported
     */
    enum AnimatedVersion : int {
        TEXSUNKN = -1,
        TEXS0002 = 0,
        TEXS0003 = 1,
    };

    enum FreeImageFormat : int {
        FIF_UNKNOWN = -1,
        FIF_BMP		= 0,
        FIF_ICO		= 1,
        FIF_JPEG	= 2,
        FIF_JNG		= 3,
        FIF_KOALA	= 4,
        FIF_LBM		= 5,
        FIF_IFF         = FIF_LBM,
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
        FIF_GIF         = 25,
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
        FIF_MP4         = FIF_WEBP,
        FIF_JXR		= 36
    };

    /**
     * Texture mipmap data
     */
    class TextureMipmap {
      public:
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
        std::unique_ptr<char[]> compressedData = nullptr;
        /** Pointer to the uncompressed data */
        std::unique_ptr<char[]> uncompressedData = nullptr;
        /** JSON data */
        std::string json {};
        /**
         * Performs actual decompression of the compressed data
         */
        void decompressData ();
    };

    /**
     * Texture header data
     */
    class TextureHeader {
      public:
        TextureHeader ();
        ~TextureHeader () = default;

        [[nodiscard]] bool isAnimated () const;

        /** The version of the texture container */
        ContainerVersion containerVersion = ContainerVersion::UNKNOWN;
        /** The version of the animated data */
        AnimatedVersion animatedVersion = AnimatedVersion::TEXSUNKN;
        /** Flags with extra texture information */
        TextureFlags flags = TextureFlags::NoFlags;
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
        TextureFormat format = TextureFormat::UNKNOWN;
        /** Free Image format */
        FreeImageFormat freeImageFormat = FreeImageFormat::FIF_UNKNOWN;
        /** Indicates if we have an MP4 video */
        bool isVideoMp4 = false;
        /** The amount of images in the texture file */
        uint32_t imageCount = 0;
        /** Number of mipmap levels on the texture */
        uint32_t mipmapCount = 0;
        /** List of mipmaps */
        std::map<uint32_t, std::vector<std::shared_ptr<TextureMipmap>>> images {};
        /** List of animation frames */
        std::vector<std::shared_ptr<TextureFrame>> frames {};
    };

  public:
    explicit CTexture (const std::shared_ptr<const uint8_t[]>& fileData);

    [[nodiscard]] GLuint getTextureID (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureWidth (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureHeight (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getRealWidth () const override;
    [[nodiscard]] uint32_t getRealHeight () const override;
    [[nodiscard]] TextureFormat getFormat () const override;
    [[nodiscard]] TextureFlags getFlags () const override;
    [[nodiscard]] const glm::vec4* getResolution () const override;
    [[nodiscard]] const std::vector<std::shared_ptr<TextureFrame>>& getFrames () const override;
    [[nodiscard]] bool isAnimated () const override;

  private:
    /**
     * @return The texture header
     */
    [[nodiscard]] const TextureHeader* getHeader () const;

    /**
     * Tries to parse a header off the given data pointer
     *
     * @param fileData The point at which to start reading data off from
     * @return
     */
    static std::unique_ptr<TextureHeader> parseHeader (const char* fileData);
    /**
     * Tries to parse an animation frame off the given data pointer
     *
     * @param originalFileData The point at which to start reading data off from
     * @return
     */
    static std::shared_ptr<TextureFrame> parseAnimation (const char** originalFileData);
    /**
     * Tries to parse mipmap information off the given data pointer
     *
     * @param header The file header
     * @param fileData The point at which to start reading data off from
     * @return
     */
    static std::shared_ptr<TextureMipmap> parseMipmap (const TextureHeader* header, const char** fileData);

    /**
     * Calculate's texture's resolution vec4
     */
    void setupResolution ();
    /**
     * Determines the texture's internal storage format
     */
    GLint setupInternalFormat ();
    /**
     * Prepares openGL parameters for loading texture data
     */
    void setupOpenGLParameters (uint32_t textureID);

    /** The texture header */
    std::unique_ptr<TextureHeader> m_header = nullptr;
    /** OpenGL's texture ID */
    GLuint* m_textureID = nullptr;
    /** Resolution vector of the texture */
    glm::vec4 m_resolution {};
};
} // namespace WallpaperEngine::Assets