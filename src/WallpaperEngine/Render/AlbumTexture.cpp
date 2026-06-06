#include "AlbumTexture.h"

#include "RenderContext.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Media/MediaSource.h"
#include "stb_image.h"

using namespace WallpaperEngine::Render;

int albumtexture_read (void* user, char* data, int size) {
    auto* stream = static_cast<ReadStream*> (user);

    stream->read (data, size);

    return stream->gcount ();
}

void albumtexture_skip (void* user, int n) {
    auto* stream = static_cast<ReadStream*> (user);
    stream->seekg (n, std::ios::cur);
}

int albumtexture_eof (void* user) { return static_cast<ReadStream*> (user)->eof (); }

stbi_io_callbacks album_texture_callbacks
    = { .read = albumtexture_read, .skip = albumtexture_skip, .eof = albumtexture_eof };

AlbumTexture::AlbumTexture (RenderContext& context) : Helpers::ContextAware (context) {
    // setup a basic texture with clamping and no mipmaps
    this->m_resolution = glm::vec4 (1.0f, 1.0f, 1.0f, 1.0f);

    glGenTextures (1, &this->m_textureID);
    glBindTexture (GL_TEXTURE_2D, this->m_textureID);

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);
}

AlbumTexture::~AlbumTexture () { glDeleteTextures (1, &this->m_textureID); }

GLuint AlbumTexture::getTextureID (uint32_t imageIndex) const { return this->m_textureID; }
uint32_t AlbumTexture::getTextureWidth (uint32_t imageIndex) const { return this->m_width; }
uint32_t AlbumTexture::getTextureHeight (uint32_t imageIndex) const { return this->m_height; }
uint32_t AlbumTexture::getRealWidth () const { return this->m_width; }
uint32_t AlbumTexture::getRealHeight () const { return this->m_height; }
TextureFormat AlbumTexture::getFormat () const { return TextureFormat_ARGB8888; }
uint32_t AlbumTexture::getFlags () const { return TextureFlags_NoFlags; }
const std::vector<FrameSharedPtr>& AlbumTexture::getFrames () const { return this->m_frames; }
const glm::vec4* AlbumTexture::getResolution () const { return &this->m_resolution; }
bool AlbumTexture::isAnimated () const { return false; }
uint32_t AlbumTexture::getSpritesheetCols () const { return 1; }
uint32_t AlbumTexture::getSpritesheetRows () const { return 1; }
uint32_t AlbumTexture::getSpritesheetFrames () const { return 1; }
float AlbumTexture::getSpritesheetDuration () const { return 0.0f; }

void AlbumTexture::incrementUsageCount () const { }
void AlbumTexture::decrementUsageCount () const { }
void AlbumTexture::update () const { }

void AlbumTexture::swap (const AlbumTexture& other) const noexcept {
    uint8_t* pixels = new uint8_t[other.m_width * other.m_height * 4];

    // copy over the other texture data into this one
    glBindTexture (GL_TEXTURE_2D, other.m_textureID);
    glGetnTexImage (GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, other.m_width * other.m_height * 4, pixels);

    glBindTexture (GL_TEXTURE_2D, this->m_textureID);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, other.m_width, other.m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // copy over the important metadata
    this->m_width = other.m_width;
    this->m_height = other.m_height;
    this->m_resolution = other.m_resolution;

    delete[] pixels;
}

void AlbumTexture::load (Media::MediaSource::MediaInfo& data) const {
    for (const auto& project : this->getContext ().getApp ().getBackgrounds () | std::views::values) {
	try {
	    // try to open the file in any of the asset locators
	    auto contents = project->assetLocator->read ("$mediaThumbnail");

	    int width, height, channels;

	    auto* dataptr
		= stbi_load_from_callbacks (&album_texture_callbacks, contents.get (), &width, &height, &channels, 4);

	    this->m_width = width;
	    this->m_height = height;
	    this->m_resolution = glm::vec4 (this->m_width, this->m_height, this->m_width, this->m_height);

	    // setup texture contents
	    glBindTexture (GL_TEXTURE_2D, this->m_textureID);
	    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, dataptr);
	    stbi_image_free (dataptr);
	    return;
	} catch (AssetLoadException&) {
	    // shouldn't really happen, but can be ignored gracefully
	}
    }
}