#include "CTexture.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cstring>
#include <lz4.h>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace WallpaperEngine::Render;

CTexture::CTexture (TextureUniquePtr header) : m_header (std::move(header)) {
    // ensure the header is parsed
    this->setupResolution ();
    GLint internalFormat = this->setupInternalFormat();

    // allocate texture ids list
    this->m_textureID = new GLuint [this->m_header->imageCount];
    // ask opengl for the correct amount of textures
    glGenTextures (this->m_header->imageCount, this->m_textureID);

    auto imgCur = this->m_header->images.begin ();
    auto imgEnd = this->m_header->images.end ();

    for (int index = 0; imgCur != imgEnd; ++imgCur, index++) {
        this->setupOpenGLParameters (index);

        auto cur = imgCur->second.begin ();
        auto end = imgCur->second.end ();

        for (int32_t level = 0; cur != end; ++cur, level++) {
            stbi_uc* handle = nullptr;
            void* dataptr = (*cur)->uncompressedData.get ();
            int width = (*cur)->width;
            int height = (*cur)->height;
            uint32_t bufferSize = (*cur)->uncompressedSize;
            GLenum textureFormat = GL_RGBA;

            if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
                int fileChannels;

                dataptr = handle = stbi_load_from_memory (
                    reinterpret_cast <unsigned char*> ((*cur)->uncompressedData.get ()),
                    (*cur)->uncompressedSize,
                    &width,
                    &height,
                    &fileChannels,
                    4);
            } else {
                if (this->m_header->format == TextureFormat_R8) {
                    // red textures are 1-byte-per-pixel, so it's alignment has to be set manually
                    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
                    textureFormat = GL_RED;
                } else if (this->m_header->format == TextureFormat_RG88) {
                    textureFormat = GL_RG;
                }
            }

            switch (internalFormat) {
                case GL_RGBA8:
                case GL_RG8:
                case GL_R8:
                    glTexImage2D (
                        GL_TEXTURE_2D, level, internalFormat, width, height, 0, textureFormat,
                        GL_UNSIGNED_BYTE, dataptr);
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                    glCompressedTexImage2D (
                        GL_TEXTURE_2D, level, internalFormat, width, height, 0, bufferSize,
                        dataptr);
                    break;
                default: sLog.exception ("Cannot load texture, unknown format", this->m_header->format);
            }

            // stbi_image buffer won't be used anymore, so free memory
            if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
                stbi_image_free (handle);
            }
        }
    }
}

void CTexture::setupResolution () {
    if (this->isAnimated ()) {
        this->m_resolution = {
            this->m_header->textureWidth, this->m_header->textureHeight,
            this->m_header->gifWidth, this->m_header->gifHeight
        };
    } else {
        if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
            // wpengine-texture format always has one mipmap
            // get first image size
            auto element = this->m_header->images.find (0)->second.begin ();

            // set the texture resolution
            this->m_resolution = {(*element)->width, (*element)->height, this->m_header->width, this->m_header->height};
        } else {
            // set the texture resolution
            this->m_resolution = {
                this->m_header->textureWidth, this->m_header->textureHeight,
                this->m_header->width, this->m_header->height
            };
        }
    }
}

GLint CTexture::setupInternalFormat () {
    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
        return GL_RGBA8;
    }

    // detect the image format and hand it to openGL to be used
    switch (this->m_header->format) {
        case TextureFormat_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
        case TextureFormat_DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
        case TextureFormat_DXT1: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
        case TextureFormat_ARGB8888: return GL_RGBA8; break;
        case TextureFormat_R8: return GL_R8; break;
        case TextureFormat_RG88: return GL_RG8; break;
        default: sLog.exception ("Cannot determine texture format");
    }
}

void CTexture::setupOpenGLParameters (uint32_t textureID) {
        // bind the texture to assign information to it
        glBindTexture (GL_TEXTURE_2D, this->m_textureID [textureID]);

        // set mipmap levels
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, this->m_header->images [textureID].size () - 1);

        // setup texture wrapping and filtering
        if (this->m_header->flags & TextureFlags_ClampUVs) {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        if (this->m_header->flags & TextureFlags_NoInterpolation) {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        } else {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }

        glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);
}

GLuint CTexture::getTextureID (uint32_t imageIndex) const {
    // ensure we do not go out of bounds
    if (imageIndex >= this->m_header->imageCount)
        return this->m_textureID [0];

    return this->m_textureID [imageIndex];
}

uint32_t CTexture::getTextureWidth (uint32_t imageIndex) const {
    if (imageIndex >= this->m_header->imageCount)
        return this->getHeader ().textureWidth;

    return (*this->m_header->images [imageIndex].begin ())->width;
}

uint32_t CTexture::getTextureHeight (uint32_t imageIndex) const {
    if (imageIndex >= this->m_header->imageCount)
        return this->getHeader ().textureHeight;

    return (*this->m_header->images [imageIndex].begin ())->height;
}

uint32_t CTexture::getRealWidth () const {
    return this->isAnimated () ? this->getHeader ().gifWidth : this->getHeader ().width;
}

uint32_t CTexture::getRealHeight () const {
    return this->isAnimated () ? this->getHeader ().gifHeight : this->getHeader ().height;
}

TextureFormat CTexture::getFormat () const {
    return this->getHeader ().format;
}

uint32_t CTexture::getFlags () const {
    return this->getHeader ().flags;
}

const Texture& CTexture::getHeader () const {
    return *this->m_header;
}

const glm::vec4* CTexture::getResolution () const {
    return &this->m_resolution;
}

const std::vector<FrameSharedPtr>& CTexture::getFrames () const {
    return this->getHeader ().frames;
}

bool CTexture::isAnimated () const {
    return this->getHeader ().isAnimated ();
}
