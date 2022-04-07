#include "CTexture.h"

#include <string>
#include <cstring>
#include <lz4.h>

using namespace WallpaperEngine::Assets;

CTexture::CTexture (void* fileData)
{
    // ensure the header is parsed
    this->m_header = this->parseHeader (static_cast <char*> (fileData));

    GLint internalFormat;

    if (this->isAnimated () == true)
    {
        this->m_resolution = {
            this->m_header->textureWidth, this->m_header->textureHeight,
            this->m_header->gifWidth, this->m_header->gifHeight
        };
    }
    else
    {
        // set the texture resolution
        this->m_resolution = {
            this->m_header->textureWidth, this->m_header->textureHeight,
            this->m_header->width, this->m_header->height
        };
    }

    if (this->m_header->freeImageFormat != FREE_IMAGE_FORMAT::FIF_UNKNOWN)
    {
        internalFormat = GL_RGBA8;
        // set some extra information too as it's used for image sizing
        // this ensures that a_TexCoord uses the full image instead of just part of it
        // TODO: MAYBE IT'S BETTER TO CREATE A TEXTURE OF THE GIVEN SIZE AND COPY OVER WHAT WE READ FROM THE FILE?
        /*this->m_header->width = this->m_header->mipmaps [0]->width;
        this->m_header->height = this->m_header->mipmaps [0]->height;
        this->m_header->textureWidth = this->m_header->mipmaps [0]->width;
        this->m_header->textureHeight = this->m_header->mipmaps [0]->height;*/
    }
    else
    {
        // detect the image format and hand it to openGL to be used
        switch (this->m_header->format)
        {
            case TextureFormat::DXT5:
                internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                break;
            case TextureFormat::DXT3:
                internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                break;
            case TextureFormat::DXT1:
                internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                break;
            case TextureFormat::ARGB8888:
                internalFormat = GL_RGBA8;
                break;
            case TextureFormat::R8:
                internalFormat = GL_R8;
                break;
            case TextureFormat::RG88:
                internalFormat = GL_RG8;
                break;
            default:
                delete this->m_header;
                throw std::runtime_error ("Cannot determine the texture format");
        }
    }

    // allocate texture ids list
    this->m_textureID = new GLuint [this->m_header->imageCount];
    // ask opengl for the correct amount of textures
    glGenTextures (this->m_header->imageCount, this->m_textureID);

    auto imgCur = this->m_header->images.begin ();
    auto imgEnd = this->m_header->images.end ();

    for (int index = 0; imgCur != imgEnd; imgCur ++, index ++)
    {
        // bind the texture to assign information to it
        glBindTexture (GL_TEXTURE_2D, this->m_textureID [index]);
        // set mipmap levels
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, this->m_header->mipmapCount - 1);

        // TODO: ADD SUPPORT FOR .tex-json FILES AS THEY ALSO HAVE FLAGS LIKE THESE ONES
        // setup texture wrapping and filtering
        if (this->m_header->flags & TextureFlags::ClampUVs)
        {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else
        {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        if (this->m_header->flags & TextureFlags::NoInterpolation)
        {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        }
        else
        {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }

        glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);

        // TODO: USE THIS ONE
        // uint32_t blockSize = (internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;

        auto cur = (*imgCur).second.begin ();
        auto end = (*imgCur).second.end ();

        for (int32_t level = 0; cur != end; cur ++, level ++)
        {
            FIBITMAP* bitmap = nullptr;
            FIBITMAP* converted = nullptr;
            FIMEMORY* memory = nullptr;
            void* dataptr = (*cur)->uncompressedData;
            uint32_t width = (*cur)->width;
            uint32_t height = (*cur)->height;
            GLenum textureFormat = GL_RGBA;

            if (this->m_header->freeImageFormat != FREE_IMAGE_FORMAT::FIF_UNKNOWN)
            {
                memory = FreeImage_OpenMemory (reinterpret_cast <BYTE *> ((*cur)->uncompressedData), (*cur)->uncompressedSize);

                // load the image and setup pointers so they can be used
                bitmap = FreeImage_LoadFromMemory (this->m_header->freeImageFormat, memory);
                // flip the image vertically
                FreeImage_FlipVertical (bitmap);
                // convert to a 32bits bytearray
                converted = FreeImage_ConvertTo32Bits (bitmap);

                dataptr = FreeImage_GetBits (converted);
                width = FreeImage_GetWidth (converted);
                height = FreeImage_GetHeight (converted);
                textureFormat = GL_BGRA;
            }
            else
            {
                if (this->m_header->format == TextureFormat::RG88)
                    textureFormat = GL_RG;
                else if (this->m_header->format == TextureFormat::R8)
                    textureFormat = GL_RED;
            }

            switch (internalFormat)
            {
                case GL_RGBA8:
                case GL_RG8:
                case GL_R8:
                    glTexImage2D (GL_TEXTURE_2D, level, internalFormat,
                                  width, height, 0,
                                  textureFormat, GL_UNSIGNED_BYTE,
                                  dataptr
                    );
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                    glCompressedTexImage2D (
                            GL_TEXTURE_2D, level, internalFormat,
                            (*cur)->width, (*cur)->height, 0,
                            (*cur)->uncompressedSize, dataptr
                    );
                    break;
            }

            // freeimage buffer won't be used anymore, so free memory
            if (this->m_header->freeImageFormat != FREE_IMAGE_FORMAT::FIF_UNKNOWN)
            {
                FreeImage_Unload (bitmap);
                FreeImage_Unload (converted);
                FreeImage_CloseMemory (memory);
            }
        }
    }
}

CTexture::~CTexture ()
{
    if (this->getHeader () == nullptr)
        return;

    // free the header if it was allocated
    delete this->getHeader ();
}

const GLuint CTexture::getTextureID (uint32_t imageIndex) const
{
    // ensure we do not go out of bounds
    if (imageIndex > this->m_header->imageCount)
        return this->m_textureID [0];

    return this->m_textureID [imageIndex];
}

const uint32_t CTexture::getTextureWidth (uint32_t imageIndex) const
{
    if (imageIndex > this->m_header->imageCount)
        return this->getHeader ()->textureWidth;

    return (*this->m_header->images [imageIndex].begin ())->width;
}

const uint32_t CTexture::getTextureHeight (uint32_t imageIndex) const
{
    if (imageIndex > this->m_header->imageCount)
        return this->getHeader ()->textureHeight;

    return (*this->m_header->images [imageIndex].begin ())->height;
}

const uint32_t CTexture::getRealWidth () const
{
    return this->isAnimated () == true ? this->getHeader ()->gifWidth : this->getHeader ()->width;
}

const uint32_t CTexture::getRealHeight () const
{
    return this->isAnimated () == true ? this->getHeader ()->gifHeight : this->getHeader ()->height;
}

const ITexture::TextureFormat CTexture::getFormat () const
{
    return this->getHeader ()->format;
}

const CTexture::TextureHeader* CTexture::getHeader () const
{
    return this->m_header;
}

const glm::vec4* CTexture::getResolution () const
{
    return &this->m_resolution;
}

const std::vector<ITexture::TextureFrame*>& CTexture::getFrames () const
{
    return this->getHeader ()->frames;
}

const bool CTexture::isAnimated () const
{
    return this->getHeader ()->isAnimated ();
}

CTexture::TextureMipmap::TextureMipmap ()
{
}

CTexture::TextureMipmap::~TextureMipmap ()
{
    if (this->compression == 1)
        delete this->compressedData;

    delete this->uncompressedData;
}

void CTexture::TextureMipmap::decompressData ()
{
    if (this->compression == 1)
    {
        this->uncompressedData = new char [this->uncompressedSize];

        int result = LZ4_decompress_safe (
            this->compressedData, this->uncompressedData,
            this->compressedSize, this->uncompressedSize
        );

        if (!result)
            throw std::runtime_error ("Cannot decompress texture data");
    }
}

CTexture::TextureFrame::TextureFrame ()
{
}

CTexture::TextureFrame::~TextureFrame ()
{
}

CTexture::TextureHeader::TextureHeader ()
{
}

CTexture::TextureHeader::~TextureHeader ()
{
    auto imgCur = this->images.begin ();
    auto imgEnd = this->images.end ();

    for (; imgCur != imgEnd; imgCur ++)
    {
        auto cur = (*imgCur).second.begin ();
        auto end = (*imgCur).second.end ();

        for (; cur != end; cur ++)
            delete *cur;
    }
}

CTexture::TextureHeader* CTexture::parseHeader (char* fileData)
{
    // check the magic value on the header first
    if (memcmp (fileData, "TEXV0005", 9) != 0)
        throw std::runtime_error ("unexpected texture container type");
    // jump to the next value
    fileData += 9;
    // check the sub-magic value on the header
    if (memcmp (fileData, "TEXI0001", 9) != 0)
        throw std::runtime_error ("unexpected texture sub-container type");
    // jump through the string again
    fileData += 9;

    TextureHeader* header = new TextureHeader;

    uint32_t* pointer = reinterpret_cast <uint32_t*> (fileData);

    header->format = static_cast <TextureFormat>(*pointer ++);
    header->flags = static_cast <TextureFlags> (*pointer ++);
    header->textureWidth = *pointer ++;
    header->textureHeight = *pointer ++;
    header->width = *pointer ++;
    header->height = *pointer ++;
    pointer ++; // ignore some more bytes

    // now we're going to parse some more data that is string
    // so get the current position back as string
    fileData = reinterpret_cast <char*> (pointer);
    // get the position of what comes after the texture data
    pointer = reinterpret_cast <uint32_t*> (fileData + 9);

    if (memcmp (fileData, "TEXB0003", 9) == 0)
    {
        header->containerVersion = ContainerVersion::TEXB0003;
        header->imageCount = *pointer ++;
        header->freeImageFormat = static_cast <FREE_IMAGE_FORMAT> (*pointer++);
    }
    else if(memcmp (fileData, "TEXB0002", 9) == 0)
    {
        header->containerVersion = ContainerVersion::TEXB0002;
        header->imageCount = *pointer ++;
    }
    else if (memcmp (fileData, "TEXB0001", 9) == 0)
    {
        header->containerVersion = ContainerVersion::TEXB0001;
        header->imageCount = *pointer ++;
    }
    else
    {
        delete header;
        throw std::runtime_error ("unknown texture format type");
    }

    for (uint32_t image = 0; image < header->imageCount; image ++)
    {
        // read the number of mipmaps available for this image
        header->mipmapCount = *pointer ++;
        std::vector <TextureMipmap*> mipmaps;

        fileData = reinterpret_cast <char*> (pointer);

        for (uint32_t i = 0; i < header->mipmapCount; i ++)
            mipmaps.emplace_back (parseMipmap (header, &fileData));

        // add the pixmaps back
        header->images.insert (std::pair <uint32_t, std::vector <TextureMipmap*>> (image, mipmaps));

        pointer = reinterpret_cast <uint32_t*> (fileData);
    }

    // gifs have extra information after the mipmaps
    if (header->isAnimated () == true)
    {
        if (memcmp (fileData, "TEXS0002", 9) == 0)
        {
            header->animatedVersion = AnimatedVersion::TEXS0002;
        }
        else if (memcmp (fileData, "TEXS0003", 9) == 0)
        {
            header->animatedVersion = AnimatedVersion::TEXS0003;
        }
        else
        {
            throw std::runtime_error ("found animation information of unknown type");
        }

        // get an integer pointer back to read the frame count
        pointer = reinterpret_cast <uint32_t*> (fileData + 9);
        uint32_t framecount = *pointer++;

        if (header->animatedVersion == AnimatedVersion::TEXS0003)
        {
            // ignore two extra integers as those are width and height of the git
            header->gifWidth = *pointer ++;
            header->gifHeight = *pointer ++;
        }

        // get back the pointer into filedata
        fileData = reinterpret_cast <char*> (pointer);

        while (framecount > 0)
        {
            // add the frame to the list
            header->frames.push_back (parseAnimation (header, &fileData));

            framecount --;
        }

        // ensure gif width and height is right for TEXS0002
        if (header->animatedVersion == AnimatedVersion::TEXS0002)
        {
            TextureFrame* first = *header->frames.begin ();

            header->gifWidth = first->width1;
            header->gifHeight = first->height1;
        }
    }

    return header;
}

CTexture::TextureFrame* CTexture::parseAnimation (TextureHeader* header, char** originalFileData)
{
    char* fileData = *originalFileData;
    // get back the pointer into integer
    uint32_t* pointer = reinterpret_cast <uint32_t*> (fileData);

    // start reading frame information
    TextureFrame* frame = new TextureFrame();

    frame->frameNumber = *pointer++;

    // reinterpret the pointer into float
    float* fPointer = reinterpret_cast <float*> (pointer);

    frame->frametime = *fPointer ++;
    frame->x = *fPointer ++;
    frame->y = *fPointer ++;
    frame->width1 = *fPointer ++;
    frame->width2 = *fPointer ++;
    frame->height2 = *fPointer ++;
    frame->height1 = *fPointer ++;

    // get back the pointer into fileData so it can be reused later
    *originalFileData = reinterpret_cast <char*> (fPointer);

    return frame;
}

CTexture::TextureMipmap* CTexture::parseMipmap (TextureHeader* header, char** originalFileData)
{
    TextureMipmap* mipmap = new TextureMipmap ();

    // get the current position
    char* fileData = *originalFileData;

    // get an integer pointer
    uint32_t* pointer = reinterpret_cast <uint32_t*> (fileData);

    mipmap->width = *pointer++;
    mipmap->height = *pointer++;

    if (header->containerVersion == ContainerVersion::TEXB0002 ||
        header->containerVersion == ContainerVersion::TEXB0003)
    {
        mipmap->compression = *pointer++;
        mipmap->uncompressedSize = *pointer++;
    }

    mipmap->compressedSize = *pointer++;

    // get back a normal char pointer
    fileData = reinterpret_cast <char*> (pointer);

    if (mipmap->compression == 0)
    {
        // this might be better named as mipmap_bytes_size instead of compressedSize
        // as in uncompressed files this variable actually holds the file length
        mipmap->uncompressedSize = mipmap->compressedSize;
    }

    mipmap->uncompressedData = new char [mipmap->uncompressedSize];

    if (mipmap->compression == 1)
    {
        mipmap->compressedData = new char [mipmap->compressedSize];

        memcpy (mipmap->compressedData, fileData, mipmap->compressedSize);

        mipmap->decompressData ();
        // advance to the end of the mipmap
        fileData += mipmap->compressedSize;
    }
    else
    {
        memcpy (mipmap->uncompressedData, fileData, mipmap->uncompressedSize);
        // advance to the end of the mipmap
        fileData += mipmap->uncompressedSize;
    }

    // ensure the pointer is updated with the latest position when reading the data
    *originalFileData = fileData;

    return mipmap;
}

const bool CTexture::TextureHeader::isAnimated () const
{
    return this->flags & TextureFlags::IsGif;
}