#include "CTexture.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cstring>
#include <lz4.h>

#define STB_IMAGE_IMPLEMENTATION
#include "RenderContext.h"

#include <stb_image.h>
#include <mpv/stream_cb.h>

using namespace WallpaperEngine::Render;

// TODO: CLEANUP CODE HERE, EXTRACT VIDEO-SPECIFIC MPV STUFF TO EXTERNAL LIBRARIES SO THIS CODE IS NOT DUPLICATED ANYMORE
struct MipmapMemoryStream : MemoryStream {
    explicit MipmapMemoryStream (const MipmapSharedPtr& mipmap) :
        MemoryStream (std::unique_ptr<char[]> (new char[mipmap->uncompressedSize]), mipmap->uncompressedSize),
        m_mipmap (mipmap) {
        // copy over the data
        // TODO: MEMORYSTREAM AND MIPMAP EXPECT A UNIQUE_PTR, SO WE CANNOT JUST USE THE POINTER DIRECTLY
        // TODO: MAYBE LOOK INTO HOW TO IMPROVE THIS?
        // TODO: MAYBE BUILD SOME KIND OF MEMORYSTREAM WITH VIEWS?
        memcpy(this->m_buffer.get(), m_mipmap->uncompressedData.get(), m_mipmap->uncompressedSize);
    }

    MipmapSharedPtr m_mipmap;
};

void* get_proc_address_texture (void* ctx, const char* name) {
    return static_cast<CTexture*> (ctx)->getContext ().getDriver ().getProcAddress (name);
}

int64_t mem_seek(void* cookie, const int64_t offset) {
    static_cast<MipmapMemoryStream*> (cookie)->seekg (offset, std::ios_base::beg);
    return static_cast<MipmapMemoryStream*> (cookie)->tellg ();
}

int64_t mem_read (void* cookie, char* buf, uint64_t bytes) {
    return static_cast<MipmapMemoryStream*> (cookie)->read (buf, bytes).gcount ();
}

int64_t mem_size (void* cookie) {
    return static_cast<MipmapMemoryStream*> (cookie)->m_mipmap->uncompressedSize;
}

void mem_close (void* cookie) {
    // free the mipmap memory stream
    delete static_cast<MipmapMemoryStream*> (cookie);
}

int mem_open(void* userdata, char* uri, struct mpv_stream_cb_info* info) {
    // no-op, video is already open, each mpv instance only supports ONE video
    // so any mem_open call is marked as okay
    // TODO: ENSURE THIS KIND OF THING IS SAFE...
    info->cookie = new MipmapMemoryStream(*static_cast<MipmapSharedPtr*>(userdata));
    info->read_fn = mem_read;
    info->seek_fn = mem_seek;
    info->close_fn = mem_close;
    info->size_fn = mem_size;

    return 0;
}

CTexture::CTexture (RenderContext& context, TextureUniquePtr header) :
    Helpers::ContextAware(context),
    m_header(std::move (header)) {
    // ensure the header is parsed
    this->setupResolution ();
    const GLint internalFormat = this->setupInternalFormat ();

    // videos are a bit special, they only have one framebuffer, one mipmap
    if (this->m_header->isVideoMp4 || this->m_header->flags & TextureFlags_Video) {
        this->m_textureID = new GLuint[1];
        glGenFramebuffers (1, &this->m_framebuffer);
        // generate the framebuffer
        glBindFramebuffer (GL_FRAMEBUFFER, this->m_framebuffer);

        this->m_videoWidth = this->m_header->textureWidth;
        this->m_videoHeight = this->m_header->textureHeight;
        // give texture a size and content
        glGenTextures (1, this->m_textureID);
        this->setupOpenGLParameters (0);
        glTexImage2D (GL_TEXTURE_2D, 0, internalFormat, this->m_videoWidth, this->m_videoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        constexpr GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        // set the texture as the colour attachmend #0
        glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_textureID[0], 0);
        // finally set the list of draw buffers
        glDrawBuffers (1, drawBuffers);

        // ensure first framebuffer is okay
        if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            sLog.exception ("Framebuffers are not properly set");
        }

        // clear the framebuffer
        glClear (GL_COLOR_BUFFER_BIT);

        // finally initialize mpv
	// setup mpv and start the render cycle
	this->m_mpv = mpv_create ();

	if (this->m_mpv == nullptr) {
	    sLog.exception ("Cannot create mpv context for video texture");
	}

	// copied off CVideo, we don't really need anything fancy here
	mpv_set_option_string (this->m_mpv, "terminal", "yes");
#if NDEBUG
	mpv_set_option_string (this->m_mpv, "msg-level", "all=status,statusline=no");
#else
	mpv_set_option_string (this->m_mpv, "msg-level", "all=v");
#endif
	mpv_set_option_string (this->m_mpv, "input-cursor", "no");
	mpv_set_option_string (this->m_mpv, "cursor-autohide", "no");
	mpv_set_option_string (this->m_mpv, "config", "no");
	mpv_set_option_string (this->m_mpv, "fbo-format", "rgba8");
	mpv_set_option_string (this->m_mpv, "vo", "libmpv");

	if (mpv_initialize (this->m_mpv) < 0) {
	    sLog.exception ("Could not initialize mpv context");
	}

	// add custom protocol
	mpv_stream_cb_add_ro(this->m_mpv, "texture", &*this->m_header->images.begin ()->second.begin(), mem_open);

	// ensure video is muted and plays in a loop
	mpv_set_option_string (this->m_mpv, "hwdec", "auto");
	mpv_set_option_string (this->m_mpv, "loop", "inf");
	mpv_set_option (this->m_mpv, "volume", MPV_FORMAT_DOUBLE, &this->m_volume);

	// initialize gl context for mpv
	mpv_opengl_init_params gl_init_params { get_proc_address_texture, this };
	mpv_render_param params[] { { MPV_RENDER_PARAM_API_TYPE, const_cast<char*> (MPV_RENDER_API_TYPE_OPENGL) },
                                    { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params },
                                    { MPV_RENDER_PARAM_INVALID, nullptr } };

	if (mpv_render_context_create (&this->m_mpvGl, this->m_mpv, params) < 0) {
	    sLog.exception ("Failed to initialize MPV's GL context");
	}

	// start playing the video
	const char* command[] = { "loadfile", "texture://", nullptr };

	if (mpv_command (this->m_mpv, command) < 0) {
	    sLog.exception ("Cannot load video texture to play");
	}

	// also mute the video
	const char* mutecommand[] = { "set", "mute", "yes", nullptr };

	mpv_command (this->m_mpv, mutecommand);
        return;
    }

    // allocate texture ids list
    this->m_textureID = new GLuint[this->m_header->imageCount];
    // ask opengl for the correct amount of textures and framebuffers
    glGenTextures (this->m_header->imageCount, this->m_textureID);

    for (const auto& [index, mipmaps] : this->m_header->images) {
	this->setupOpenGLParameters (index);

	int level = 0;

	for (const auto& mipmap : mipmaps) {
	    stbi_uc* handle = nullptr;
	    const void* dataptr = mipmap->uncompressedData.get ();
	    int width = mipmap->width;
	    int height = mipmap->height;
	    const uint32_t bufferSize = mipmap->uncompressedSize;
	    GLenum textureFormat = GL_RGBA;

	    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
		int fileChannels;

		dataptr = handle = stbi_load_from_memory (
		    reinterpret_cast<unsigned char*> (mipmap->uncompressedData.get ()), mipmap->uncompressedSize,
		    &width, &height, &fileChannels, 4
		);
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
			GL_TEXTURE_2D, level, internalFormat, width, height, 0, textureFormat, GL_UNSIGNED_BYTE, dataptr
		    );
		    break;
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		    glCompressedTexImage2D (
			GL_TEXTURE_2D, level, internalFormat, width, height, 0, bufferSize, dataptr
		    );
		    break;
		default:
		    sLog.exception ("Cannot load texture, unknown format", this->m_header->format);
	    }

	    // stbi_image buffer won't be used anymore, so free memory
	    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
		stbi_image_free (handle);
	    }

	    level++;
	}
    }
}

CTexture::~CTexture () {
    if (this->m_mpvGl) {
        mpv_render_context_free (this->m_mpvGl);
        this->m_mpvGl = nullptr;
    }

    if (this->m_mpv) {
        mpv_terminate_destroy (this->m_mpv);
        this->m_mpv = nullptr;
    }

    if (this->m_framebuffer != GL_NONE) {
        glDeleteFramebuffers (1, &this->m_framebuffer);
    }

    glDeleteTextures (this->m_header->imageCount, this->m_textureID);

    delete[] this->m_textureID;
}

void CTexture::setupResolution () {
    if (this->isAnimated ()) {
	this->m_resolution = { this->m_header->textureWidth, this->m_header->textureHeight, this->m_header->gifWidth,
			       this->m_header->gifHeight };
    } else {
	if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
	    // wpengine-texture format always has one mipmap
	    // get first image size
	    const auto element = this->m_header->images.find (0)->second.begin ();

	    // set the texture resolution
	    this->m_resolution
		= { (*element)->width, (*element)->height, this->m_header->width, this->m_header->height };
	} else {
	    // set the texture resolution
	    this->m_resolution = { this->m_header->textureWidth, this->m_header->textureHeight, this->m_header->width,
				   this->m_header->height };
	}
    }
}

GLint CTexture::setupInternalFormat () const {
    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
	return GL_RGBA8;
    }

    // detect the image format and hand it to openGL to be used
    switch (this->m_header->format) {
	case TextureFormat_DXT5:
	    return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	case TextureFormat_DXT3:
	    return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
	case TextureFormat_DXT1:
	    return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	case TextureFormat_ARGB8888:
	    return GL_RGBA8;
	case TextureFormat_R8:
	    return GL_R8;
	case TextureFormat_RG88:
	    return GL_RG8;
	default:
	    sLog.exception ("Cannot determine texture format");
    }
}

void CTexture::setupOpenGLParameters (const uint32_t textureID) const {
    // TODO: LABEL ELEMENTS TOO
    // bind the texture to assign information to it
    glBindTexture (GL_TEXTURE_2D, this->m_textureID[textureID]);

    // set mipmap levels
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, this->m_header->images[textureID].size () - 1);

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

GLuint CTexture::getTextureID (const uint32_t imageIndex) const {
    // ensure we do not go out of bounds
    if (imageIndex >= this->m_header->imageCount) {
	return this->m_textureID[0];
    }

    return this->m_textureID[imageIndex];
}

uint32_t CTexture::getTextureWidth (const uint32_t imageIndex) const {
    if (imageIndex >= this->m_header->imageCount) {
	return this->getHeader ().textureWidth;
    }

    return (*this->m_header->images[imageIndex].begin ())->width;
}

uint32_t CTexture::getTextureHeight (const uint32_t imageIndex) const {
    if (imageIndex >= this->m_header->imageCount) {
	return this->getHeader ().textureHeight;
    }

    return (*this->m_header->images[imageIndex].begin ())->height;
}

uint32_t CTexture::getRealWidth () const {
    return this->isAnimated () ? this->getHeader ().gifWidth : this->getHeader ().width;
}

uint32_t CTexture::getRealHeight () const {
    return this->isAnimated () ? this->getHeader ().gifHeight : this->getHeader ().height;
}

TextureFormat CTexture::getFormat () const { return this->getHeader ().format; }

uint32_t CTexture::getFlags () const { return this->getHeader ().flags; }

const Texture& CTexture::getHeader () const { return *this->m_header; }

const glm::vec4* CTexture::getResolution () const { return &this->m_resolution; }

const std::vector<FrameSharedPtr>& CTexture::getFrames () const { return this->getHeader ().frames; }

bool CTexture::isAnimated () const { return this->getHeader ().isAnimated (); }

uint32_t CTexture::getSpritesheetCols () const { return this->getHeader ().spritesheetCols; }

uint32_t CTexture::getSpritesheetRows () const { return this->getHeader ().spritesheetRows; }

uint32_t CTexture::getSpritesheetFrames () const { return this->getHeader ().spritesheetFrames; }

float CTexture::getSpritesheetDuration () const { return this->getHeader ().spritesheetDuration; }

void CTexture::update () const {
    // do not do anything if the texture is not a video
    if (this->m_mpv == nullptr) {
        return;
    }

    // video textures should only have one mipmap
    const auto mipmap = *this->getHeader ().images.begin ()->second.begin ();
    // read all the events available
    while (true) {
        const mpv_event* event = mpv_wait_event (this->m_mpv, 0);

        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            break;
        }

        // we do not care about any of the events
        if (event->event_id == MPV_EVENT_VIDEO_RECONFIG) {
            if (mpv_get_property (this->m_mpv, "dwidth", MPV_FORMAT_INT64, &this->m_videoWidth) >= 0
                && mpv_get_property (this->m_mpv, "dheight", MPV_FORMAT_INT64, &this->m_videoHeight) >= 0) {
                // reconfigure the texture
                glBindTexture (GL_TEXTURE_2D, this->m_textureID[0]);
                glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, this->m_videoWidth, this->m_videoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }
        }
    }

    // render the next
    glViewport (0, 0, this->m_videoWidth, this->m_videoHeight);

    mpv_opengl_fbo fbo { static_cast<int> (this->m_framebuffer), static_cast<int> (this->m_videoWidth),
                         static_cast<int> (this->m_videoHeight), GL_RGBA8 };

    // no need to flip as it'll be handled by the wallpaper rendering code
    int flip_y = 0;

    mpv_render_param params[] = { { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
                                  { MPV_RENDER_PARAM_FLIP_Y, &flip_y },
                                  { MPV_RENDER_PARAM_INVALID, nullptr } };

    mpv_render_context_render (this->m_mpvGl, params);
}