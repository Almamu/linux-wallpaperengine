#include "CTexture.h"

#include <string>
#include <cstring>
#include <lz4.h>

using namespace WallpaperEngine::Assets;

CTexture::CTexture (void* fileData)
{
    TextureHeader* header = this->parseHeader (static_cast <char*> (fileData));

    if (header->freeImageFormat != FREE_IMAGE_FORMAT::FIF_UNKNOWN)
        throw std::runtime_error ("Normal images are not supported yet");

    GLint formatGL;

    // detect the image format and hand it to openGL to be used
    switch (header->format)
    {
        case TextureFormat::DXT5:
            formatGL = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            break;
        case TextureFormat::DXT3:
            formatGL = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            break;
        case TextureFormat::DXT1:
            formatGL = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            break;
        case TextureFormat::ARGB8888:
            formatGL = GL_RGBA8;
            break;
        default:
            delete header;
            throw std::runtime_error ("Cannot determine the texture format");
    }

    // reserve a texture
    glGenTextures (1, &this->m_textureID);

    // bind the texture to assign information to it
    glBindTexture (GL_TEXTURE_2D, this->m_textureID);
    // set mipmap levels
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, header->mipmapCount - 1);

    // setup texture wrapping and filtering
    if (header->flags & TextureFlags::ClampUVs)
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    if (header->flags & TextureFlags::NoInterpolation)
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    // TODO: USE THIS ONE
    uint32_t blockSize = (formatGL == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;

    auto cur = header->mipmaps.begin ();
    auto end = header->mipmaps.end ();

    for (int32_t level = 0; cur != end; cur ++, level ++)
    {
        switch (formatGL)
        {
            case GL_RGBA8:
                glTexImage2D (GL_TEXTURE_2D, level, formatGL,
                    (*cur)->width, (*cur)->height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE,
                    (*cur)->uncompressedData
                );
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                glCompressedTexImage2D (
                    GL_TEXTURE_2D, level, formatGL,
                    (*cur)->width, (*cur)->height, 0,
                    (*cur)->uncompressedSize, (*cur)->uncompressedData
                );
                break;

        }
    }

    delete header;
    // TODO: IMPLEMENT SUPPORT FOR NORMAL IMAGES
}

CTexture::~CTexture ()
{
    // TODO: FREE STUFF HERE
}

const GLuint CTexture::getTextureID () const
{
    return this->m_textureID;
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

CTexture::TextureHeader::TextureHeader ()
{
}

CTexture::TextureHeader::~TextureHeader ()
{
    auto cur = this->mipmaps.begin ();
    auto end = this->mipmaps.end ();

    for (; cur != end; cur ++)
        delete *cur;
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

    uint32_t* pointer = reinterpret_cast<uint32_t*> (fileData);

    header->format = static_cast<TextureFormat>(*pointer ++);
    header->flags = static_cast<TextureFlags> (*pointer ++);
    header->textureWidth = *pointer ++;
    header->textureHeight = *pointer ++;
    header->width = *pointer ++;
    header->height = *pointer ++;
    pointer ++; // ignore some more bytes

    // now we're going to parse some more data that is string
    // so get the current position back as string
    fileData = reinterpret_cast <char*> (pointer);
    // get the position of what comes after the texture data
    char* afterFileData = fileData + 9;

    if (memcmp (fileData, "TEXB0003", 9) == 0)
    {
        header->containerVersion = ContainerVersion::TEXB0003;

        // get back the pointer and use it
        pointer = reinterpret_cast <uint32_t*> (afterFileData);
        pointer ++;
        header->freeImageFormat = static_cast<FREE_IMAGE_FORMAT> (*pointer++);
        // set back the pointer
        fileData = reinterpret_cast <char*> (pointer);
    }
    else if(memcmp (fileData, "TEXB0002", 9) == 0)
    {
        header->containerVersion = ContainerVersion::TEXB0002;

        // get back the pointer and use it
        pointer = reinterpret_cast <uint32_t*> (afterFileData);
        pointer ++;
        // set back the pointer
        fileData = reinterpret_cast <char*> (pointer);
    }
    else if (memcmp (fileData, "TEXB0001", 9) == 0)
    {
        header->containerVersion = ContainerVersion::TEXB0001;

        // get back the pointer and use it
        pointer = reinterpret_cast <uint32_t*> (afterFileData);
        pointer ++;
        // set back the pointer
        fileData = reinterpret_cast <char*> (pointer);
    }
    else
    {
        delete header;
        throw std::runtime_error ("unknown texture format type");
    }

    if (header->format == TextureFormat::R8)
    {
        delete header;

        throw std::runtime_error ("R8 format is not supported yet");
    }
    else if (header->format == TextureFormat::RG88)
    {
        delete header;

        throw std::runtime_error ("RG88 format is not supported yet");
    }

    // get back an integer pointer
    pointer = reinterpret_cast <uint32_t*> (fileData);

    // read the number of mipmaps available
    header->mipmapCount = *pointer ++;

    fileData = reinterpret_cast <char*> (pointer);

    for (uint32_t i = 0; i < header->mipmapCount; i ++)
        header->mipmaps.emplace_back (this->parseMipmap (header, &fileData));

    return header;
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