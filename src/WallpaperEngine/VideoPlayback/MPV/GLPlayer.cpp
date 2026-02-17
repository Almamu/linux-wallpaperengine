#include "GLPlayer.h"

#include "WallpaperEngine/Logging/Log.h"

#include <mpv/render_gl.h>
#include <mpv/stream_cb.h>

using namespace WallpaperEngine::VideoPlayback::MPV;

void* get_proc_address (void* ctx, const char* name) {
    return static_cast<GLPlayer*> (ctx)->getContext ().getDriver ().getProcAddress (name);
}

GLPlayer::GLPlayer (
    RenderContext& context, GLuint outputTexture, const std::filesystem::path& file, const int64_t baseWidth,
    const int64_t baseHeight, const GLuint fbo
) : ContextAware (context), m_outputTexture (outputTexture), m_width (baseWidth), m_height (baseHeight) {
    this->m_fbo = fbo;
    this->m_doWeOwnFramebuffer = this->m_fbo == GL_NONE;

    this->prepareGL ();
    this->play (file);
}

GLPlayer::GLPlayer (
    RenderContext& context, const GLuint outputTexture, MemoryStreamProtocolUniquePtr stream, const int64_t baseWidth,
    const int64_t baseHeight, const GLuint fbo
) : ContextAware (context), m_outputTexture (outputTexture), m_width (baseWidth), m_height (baseHeight) {
    this->m_fbo = fbo;
    this->m_doWeOwnFramebuffer = this->m_fbo == GL_NONE;

    this->prepareGL ();
    this->play (std::move (stream));
}

GLPlayer::~GLPlayer () {
    // clean up any kept resources
    this->stop ();

    // only clean up framebuffer if we own it
    if (this->m_doWeOwnFramebuffer) {
	// free gl resources too
	glDeleteFramebuffers (1, &this->m_fbo);
    }
}

void GLPlayer::setUntimed () {
    this->m_untimed = true;

    // apply the value to the playback if it's already started
    if (this->m_handle) {
	mpv_set_option_string (this->m_handle, "untimed", "yes");
    }
}

void GLPlayer::clearUntimed () {
    this->m_untimed = false;

    // apply the value to the playback if it's already started
    if (this->m_handle) {
	mpv_set_option_string (this->m_handle, "untimed", "no");
    }
}

void GLPlayer::setMuted () {
    this->m_muted = true;

    if (this->m_handle) {
	mpv_set_option_string (this->m_handle, "mute", "yes");
    }
}

void GLPlayer::clearMuted () {
    this->m_muted = false;

    if (this->m_handle) {
	mpv_set_option_string (this->m_handle, "mute", "no");
    }
}

void GLPlayer::setVolume (double volume) {
    this->m_volume = volume;

    if (this->m_handle) {
	mpv_set_option (this->m_handle, "volume", MPV_FORMAT_DOUBLE, &this->m_volume);
    }
}

void GLPlayer::setPaused () {
    this->m_paused = true;

    if (this->m_handle) {
	mpv_set_property (this->m_handle, "pause", MPV_FORMAT_FLAG, &this->m_paused);
    }
}

void GLPlayer::clearPaused () {
    this->m_paused = false;

    if (this->m_handle) {
	mpv_set_property (this->m_handle, "pause", MPV_FORMAT_FLAG, &this->m_paused);
    }
}

void GLPlayer::render () const {
    // do not do anything if the texture is not a video
    if (this->m_handle == nullptr) {
	return;
    }

    // read all the events available
    while (true) {
	const mpv_event* event = mpv_wait_event (this->m_handle, 0);

	if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
	    break;
	}

	// we do not care about any of the events
	if (event->event_id == MPV_EVENT_VIDEO_RECONFIG) {
	    if (mpv_get_property (this->m_handle, "dwidth", MPV_FORMAT_INT64, &this->m_width) >= 0
		&& mpv_get_property (this->m_handle, "dheight", MPV_FORMAT_INT64, &this->m_height) >= 0) {
		// reconfigure the texture
		glBindTexture (GL_TEXTURE_2D, this->m_outputTexture);
		glTexImage2D (
		    GL_TEXTURE_2D, 0, GL_RGBA8, this->m_width, this->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
		);
	    }
	}
    }

    // render the next
    glViewport (0, 0, this->m_width, this->m_height);

    mpv_opengl_fbo fbo { static_cast<int> (this->m_fbo), static_cast<int> (this->m_width),
			 static_cast<int> (this->m_height), GL_RGBA8 };

    // no need to flip as it'll be handled by the wallpaper rendering code
    int flip_y = 0;

    mpv_render_param params[] = { { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
				  { MPV_RENDER_PARAM_FLIP_Y, &flip_y },
				  { MPV_RENDER_PARAM_INVALID, nullptr } };

    mpv_render_context_render (this->m_renderContext, params);
}

int GLPlayer::getWidth () const { return this->m_width; }
int GLPlayer::getHeight () const { return this->m_height; }

void GLPlayer::prepareGL () {
    if (!this->m_doWeOwnFramebuffer || this->m_fbo != GL_NONE) {
	return;
    }

    glGenFramebuffers (1, &this->m_fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_fbo);
    glBindTexture (GL_TEXTURE_2D, this->m_outputTexture);
    // reset texture's contents
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, this->m_width, this->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    constexpr GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_outputTexture, 0);
    glDrawBuffers (1, drawBuffers);

    // ensure first framebuffer is okay
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	sLog.exception ("Framebuffers are not properly set");
    }

    // clear the framebuffer
    glClear (GL_COLOR_BUFFER_BIT);
}

void GLPlayer::init () {
    this->m_handle = mpv_create ();

    if (this->m_handle == nullptr) {
	sLog.exception ("Cannot create mpv context for video texture");
    }

    // copied off CVideo, we don't really need anything fancy here
    mpv_set_option_string (this->m_handle, "terminal", "yes");
#if NDEBUG
    mpv_set_option_string (this->m_handle, "msg-level", "all=status,statusline=no");
#else
    mpv_set_option_string (this->m_handle, "msg-level", "all=v");
#endif
    mpv_set_option_string (this->m_handle, "input-cursor", "no");
    mpv_set_option_string (this->m_handle, "cursor-autohide", "no");
    mpv_set_option_string (this->m_handle, "config", "no");
    mpv_set_option_string (this->m_handle, "fbo-format", "rgba8");
    mpv_set_option_string (this->m_handle, "vo", "libmpv");
    mpv_set_option_string (this->m_handle, "profile", "fast");
    mpv_set_option_string (this->m_handle, "untimed", this->m_untimed ? "yes" : "no");

    if (mpv_initialize (this->m_handle) < 0) {
	sLog.exception ("Could not initialize mpv context");
    }

    // ensure video is muted and plays in a loop
    mpv_set_option_string (this->m_handle, "hwdec", "auto");
    mpv_set_option_string (this->m_handle, "loop", "inf");
    mpv_set_option (this->m_handle, "volume", MPV_FORMAT_DOUBLE, &this->m_volume);

    // initialize gl context for mpv
    mpv_opengl_init_params gl_init_params { get_proc_address, this };
    mpv_render_param params[] { { MPV_RENDER_PARAM_API_TYPE, const_cast<char*> (MPV_RENDER_API_TYPE_OPENGL) },
				{ MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params },
				{ MPV_RENDER_PARAM_INVALID, nullptr } };

    if (mpv_render_context_create (&this->m_renderContext, this->m_handle, params) < 0) {
	sLog.exception ("Failed to initialize MPV's GL context");
    }

    // mute the video by default
    mpv_set_option_string (this->m_handle, "mute", this->m_muted ? "yes" : "no");
}

void GLPlayer::play (const std::filesystem::path& file) {
    if (this->m_handle == nullptr) {
	this->init ();
    }

    // build the path to the video file
    const char* command[] = { "loadfile", file.c_str (), nullptr };

    if (mpv_command (this->m_handle, command) < 0) {
	sLog.exception ("Cannot load video to play");
    }
}

void GLPlayer::play (MemoryStreamProtocolUniquePtr source) {
    if (this->m_handle == nullptr) {
	this->init ();
    }

    source->linkPlayer (*this);
    this->m_stream = std::move (source);

    // start playing the video
    const char* command[] = { "loadfile", "buffer://", nullptr };

    if (mpv_command (this->m_handle, command) < 0) {
        sLog.exception ("Cannot load video texture to play");
    }

}

void GLPlayer::stop () {
    // clean up mpv and get it ready to start again at some point
    if (this->m_renderContext) {
	mpv_render_context_free (this->m_renderContext);
	this->m_renderContext = nullptr;
    }

    if (this->m_handle) {
	mpv_terminate_destroy (this->m_handle);
	this->m_handle = nullptr;
    }
}