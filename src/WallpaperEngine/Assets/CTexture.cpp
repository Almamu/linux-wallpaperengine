#include "CTexture.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstring>
#include <lz4.h>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace WallpaperEngine::Assets;

CTexture::CTexture (const std::shared_ptr<const uint8_t[]>& buffer) : m_resolution () {
    // ensure the header is parsed
    const void* fileData = buffer.get ();
    this->m_header = parseHeader (static_cast<const char*> (fileData));
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
            void* dataptr = (*cur)->uncompressedData;
            int width = (*cur)->width;
            int height = (*cur)->height;
            uint32_t bufferSize = (*cur)->uncompressedSize;
            GLenum textureFormat = GL_RGBA;

            if (this->m_header->freeImageFormat != FreeImageFormat::FIF_UNKNOWN) {
                int fileChannels;

                dataptr = handle = stbi_load_from_memory (
                    reinterpret_cast <unsigned char*> ((*cur)->uncompressedData),
                    (*cur)->uncompressedSize,
                    &width,
                    &height,
                    &fileChannels,
                    4);
            } else {
                if (this->m_header->format == TextureFormat::R8) {
                    // red textures are 1-byte-per-pixel, so it's alignment has to be set manually
                    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
                    textureFormat = GL_RED;
                } else if (this->m_header->format == TextureFormat::RG88) {
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
            if (this->m_header->freeImageFormat != FreeImageFormat::FIF_UNKNOWN) {
                stbi_image_free (handle);
            }
        }
    }
}

void CTexture::setupResolution () {
    if (this->isAnimated ()) {
        this->m_resolution = {this->m_header->textureWidth, this->m_header->textureHeight, this->m_header->gifWidth,
                              this->m_header->gifHeight};
    } else {
        if (this->m_header->freeImageFormat != FreeImageFormat::FIF_UNKNOWN) {
            // wpengine-texture format always has one mipmap
            // get first image size
            auto element = this->m_header->images.find (0)->second.begin ();

            // set the texture resolution
            this->m_resolution = {(*element)->width, (*element)->height, this->m_header->width, this->m_header->height};
        } else {
            // set the texture resolution
            this->m_resolution = {this->m_header->textureWidth, this->m_header->textureHeight, this->m_header->width,
                                  this->m_header->height};
        }
    }
}

GLint CTexture::setupInternalFormat () {
    if (this->m_header->freeImageFormat != FreeImageFormat::FIF_UNKNOWN) {
        return GL_RGBA8;
        // set some extra information too as it's used for image sizing
        // this ensures that a_TexCoord uses the full image instead of just part of it
        // TODO: MAYBE IT'S BETTER TO CREATE A TEXTURE OF THE GIVEN SIZE AND COPY OVER WHAT WE READ FROM THE FILE?
        /*this->m_header->width = this->m_header->mipmaps [0]->width;
        this->m_header->height = this->m_header->mipmaps [0]->height;
        this->m_header->textureWidth = this->m_header->mipmaps [0]->width;
        this->m_header->textureHeight = this->m_header->mipmaps [0]->height;*/
    } else {
        // detect the image format and hand it to openGL to be used
        switch (this->m_header->format) {
            case TextureFormat::DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
            case TextureFormat::DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
            case TextureFormat::DXT1: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
            case TextureFormat::ARGB8888: return GL_RGBA8; break;
            case TextureFormat::R8: return GL_R8; break;
            case TextureFormat::RG88: return GL_RG8; break;
            default: sLog.exception ("Cannot determine texture format");
        }
    }
}

void CTexture::setupOpenGLParameters (uint32_t textureID) {
        // bind the texture to assign information to it
        glBindTexture (GL_TEXTURE_2D, this->m_textureID [textureID]);

        // set mipmap levels
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, this->m_header->mipmapCount - 1);

        // setup texture wrapping and filtering
        if (this->m_header->flags & TextureFlags::ClampUVs) {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        if (this->m_header->flags & TextureFlags::NoInterpolation) {
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
        return this->getHeader ()->textureWidth;

    return (*this->m_header->images [imageIndex].begin ())->width;
}

uint32_t CTexture::getTextureHeight (uint32_t imageIndex) const {
    if (imageIndex >= this->m_header->imageCount)
        return this->getHeader ()->textureHeight;

    return (*this->m_header->images [imageIndex].begin ())->height;
}

uint32_t CTexture::getRealWidth () const {
    return this->isAnimated () ? this->getHeader ()->gifWidth : this->getHeader ()->width;
}

uint32_t CTexture::getRealHeight () const {
    return this->isAnimated () ? this->getHeader ()->gifHeight : this->getHeader ()->height;
}

ITexture::TextureFormat CTexture::getFormat () const {
    return this->getHeader ()->format;
}

ITexture::TextureFlags CTexture::getFlags () const {
    return this->getHeader ()->flags;
}

const CTexture::TextureHeader* CTexture::getHeader () const {
    return this->m_header.get ();
}

const glm::vec4* CTexture::getResolution () const {
    return &this->m_resolution;
}

const std::vector<std::shared_ptr<ITexture::TextureFrame>>& CTexture::getFrames () const {
    return this->getHeader ()->frames;
}

bool CTexture::isAnimated () const {
    return this->getHeader ()->isAnimated ();
}

CTexture::TextureMipmap::TextureMipmap () = default;

CTexture::TextureMipmap::~TextureMipmap () {
    if (this->compression == 1) {
        delete this->compressedData;
    }

    delete this->uncompressedData;
}

void CTexture::TextureMipmap::decompressData () {
    if (this->compression != 1) {
        return;
    }

    this->uncompressedData = new char [this->uncompressedSize];

    const int result = LZ4_decompress_safe (this->compressedData, this->uncompressedData, this->compressedSize,
                                            this->uncompressedSize);

    if (!result)
        sLog.exception ("Cannot decompress texture data, LZ4_decompress_safe returned an error");
}

CTexture::TextureFrame::TextureFrame () :
    frameNumber (0),
    frametime (0.0f),
    x (0),
    y (0),
    width1 (0),
    width2 (0),
    height1 (0),
    height2 (0) {}

CTexture::TextureHeader::TextureHeader () :
    flags (NoFlags),
    width (0),
    height (0),
    textureWidth (0),
    textureHeight (0),
    gifWidth (0),
    gifHeight (0),
    format (TextureFormat::UNKNOWN),
    imageCount (0),
    mipmapCount (0),
    isVideoMp4 (false) {}

std::unique_ptr<CTexture::TextureHeader> CTexture::parseHeader (const char* fileData) {
    // check the magic value on the header first
    if (strncmp (fileData, "TEXV0005", 9) != 0)
        sLog.exception ("unexpected texture container type: ", std::string_view (fileData, 9));
    // jump to the next value
    fileData += 9;
    // check the sub-magic value on the header
    if (strncmp (fileData, "TEXI0001", 9) != 0)
        sLog.exception ("unexpected texture sub-container type: ", std::string_view (fileData, 9));
    // jump through the string again
    fileData += 9;

    auto header = std::make_unique <TextureHeader> ();
    const auto* pointer = reinterpret_cast<const uint32_t*> (fileData);

    header->format = static_cast<TextureFormat> (*pointer++);
    header->flags = static_cast<TextureFlags> (*pointer++);
    header->textureWidth = *pointer++;
    header->textureHeight = *pointer++;
    header->width = *pointer++;
    header->height = *pointer++;
    pointer++; // ignore some more bytes

    // now we're going to parse some more data that is string
    // so get the current position back as string
    fileData = reinterpret_cast<const char*> (pointer);
    // get the position of what comes after the texture data
    pointer = reinterpret_cast<const uint32_t*> (fileData + 9);

    header->imageCount = *pointer++;

    if (strncmp (fileData, "TEXB0004", 9) == 0) {
        header->containerVersion = ContainerVersion::TEXB0004;
        header->freeImageFormat = static_cast<FreeImageFormat> (*pointer++);
        header->isVideoMp4 = *pointer++ == 1;

        if (header->freeImageFormat == FIF_UNKNOWN) {
            header->freeImageFormat = FIF_MP4;
        }

        // default to TEXB0003 behavior if no mp4 video is there
        if (header->freeImageFormat != FIF_MP4) {
            header->containerVersion = ContainerVersion::TEXB0003;
        }
    } else if (strncmp (fileData, "TEXB0003", 9) == 0) {
        header->containerVersion = ContainerVersion::TEXB0003;
        header->freeImageFormat = static_cast<FreeImageFormat> (*pointer++);
    } else if (strncmp (fileData, "TEXB0002", 9) == 0) {
        header->containerVersion = ContainerVersion::TEXB0002;
    } else if (strncmp (fileData, "TEXB0001", 9) == 0) {
        header->containerVersion = ContainerVersion::TEXB0001;
    } else {
        sLog.exception ("unknown texture format type: ", std::string_view (fileData, 9));
    }

    for (uint32_t image = 0; image < header->imageCount; image++) {
        // read the number of mipmaps available for this image
        header->mipmapCount = *pointer++;
        std::vector<std::shared_ptr<TextureMipmap>> mipmaps;

        fileData = reinterpret_cast<const char*> (pointer);

        for (uint32_t i = 0; i < header->mipmapCount; i++)
            mipmaps.emplace_back (parseMipmap (header.get (), &fileData));

        // add the pixmaps back
        header->images.insert (std::pair (image, mipmaps));

        pointer = reinterpret_cast<const uint32_t*> (fileData);
    }

    // gifs have extra information after the mipmaps
    if (header->isAnimated ()) {
        if (strncmp (fileData, "TEXS0002", 9) == 0) {
            header->animatedVersion = AnimatedVersion::TEXS0002;
        } else if (strncmp (fileData, "TEXS0003", 9) == 0) {
            header->animatedVersion = AnimatedVersion::TEXS0003;
        } else {
            sLog.exception ("found animation information of unknown type: ", std::string_view (fileData, 9));
        }

        // get an integer pointer back to read the frame count
        pointer = reinterpret_cast<const uint32_t*> (fileData + 9);
        uint32_t framecount = *pointer++;

        if (header->animatedVersion == AnimatedVersion::TEXS0003) {
            // ignore two extra integers as those are width and height of the git
            header->gifWidth = *pointer++;
            header->gifHeight = *pointer++;
        }

        // get back the pointer into filedata
        fileData = reinterpret_cast<const char*> (pointer);

        while (framecount > 0) {
            // add the frame to the list
            header->frames.push_back (parseAnimation (&fileData));

            framecount--;
        }

        // ensure gif width and height is right for TEXS0002
        if (header->animatedVersion == AnimatedVersion::TEXS0002) {
            auto first = *header->frames.begin ();

            header->gifWidth = first->width1;
            header->gifHeight = first->height1;
        }
    }

    return header;
}

std::shared_ptr<CTexture::TextureFrame> CTexture::parseAnimation (const char** originalFileData) {
    const char* fileData = *originalFileData;
    // get back the pointer into integer
    const auto* pointer = reinterpret_cast<const uint32_t*> (fileData);

    // start reading frame information
    auto frame = std::make_shared <TextureFrame> ();

    frame->frameNumber = *pointer++;

    // reinterpret the pointer into float
    const auto* fPointer = reinterpret_cast<const float*> (pointer);

    frame->frametime = *fPointer++;
    frame->x = *fPointer++;
    frame->y = *fPointer++;
    frame->width1 = *fPointer++;
    frame->width2 = *fPointer++;
    frame->height2 = *fPointer++;
    frame->height1 = *fPointer++;

    // get back the pointer into fileData so it can be reused later
    *originalFileData = reinterpret_cast<const char*> (fPointer);

    return frame;
}

std::shared_ptr<CTexture::TextureMipmap> CTexture::parseMipmap (const TextureHeader* header, const char** originalFileData) {
    auto mipmap = std::make_shared <TextureMipmap> ();
    // get the current position
    const char* fileData = *originalFileData;

    // get an integer pointer
    const auto* pointer = reinterpret_cast<const uint32_t*> (fileData);

    // TEXB004 have some extra data (and even json) that we have to take into account
    if (header->containerVersion == ContainerVersion::TEXB0004) {
        // ignore various params, RePKG doesn't really use them
        // and could be related to the editor really, so just ignore them
        pointer++;
        pointer++;

        fileData = reinterpret_cast<const char*> (pointer);
        while (*fileData != 0) {
            mipmap->json += *fileData++;
        }

        // skip the null terminator
        fileData ++;

        pointer = reinterpret_cast<const uint32_t*> (fileData);
    }

    mipmap->width = *pointer++;
    mipmap->height = *pointer++;

    if (header->containerVersion == ContainerVersion::TEXB0002 ||
        header->containerVersion == ContainerVersion::TEXB0003 ||
        header->containerVersion == ContainerVersion::TEXB0004) {
        mipmap->compression = *pointer++;
        mipmap->uncompressedSize = *pointer++;
    }

    mipmap->compressedSize = *pointer++;

    // get back a normal char pointer
    fileData = reinterpret_cast<const char*> (pointer);

    if (mipmap->compression == 0) {
        // this might be better named as mipmap_bytes_size instead of compressedSize
        // as in uncompressed files this variable actually holds the file length
        mipmap->uncompressedSize = mipmap->compressedSize;
    }

    mipmap->uncompressedData = new char [mipmap->uncompressedSize];

    if (mipmap->compression == 1) {
        mipmap->compressedData = new char [mipmap->compressedSize];

        memcpy (mipmap->compressedData, fileData, mipmap->compressedSize);

        mipmap->decompressData ();
        // advance to the end of the mipmap
        fileData += mipmap->compressedSize;
    } else {
        memcpy (mipmap->uncompressedData, fileData, mipmap->uncompressedSize);
        // advance to the end of the mipmap
        fileData += mipmap->uncompressedSize;
    }

    // ensure the pointer is updated with the latest position when reading the data
    *originalFileData = fileData;

    return mipmap;
}

bool CTexture::TextureHeader::isAnimated () const {
    return this->flags & TextureFlags::IsGif;
}