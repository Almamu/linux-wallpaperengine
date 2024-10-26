#include "CVideo.h"
#include "common.h"

#include <GL/glew.h>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

void* get_proc_address (void* ctx, const char* name) {
    return static_cast<CVideo*> (ctx)->getContext ().getDriver ().getProcAddress (name);
}

CVideo::CVideo (Core::CVideo* video, CRenderContext& context, CAudioContext& audioContext,
                const CWallpaperState::TextureUVsScaling& scalingMode) :
    CWallpaper (video, Type, context, audioContext, scalingMode),
    m_width (16),
    m_height (16),
    m_paused (false),
    m_mpvGl (nullptr) {
    double volume = this->getContext ().getApp ().getContext ().settings.audio.volume * 100.0 / 128.0;

    // create mpv contexts
    this->m_mpv = mpv_create ();

    if (this->m_mpv == nullptr)
        sLog.exception ("Could not create mpv context");

    mpv_set_option_string (this->m_mpv, "terminal", "yes");
    mpv_set_option_string (this->m_mpv, "msg-level", "all=v");
    mpv_set_option_string (this->m_mpv, "input-cursor", "no");
    mpv_set_option_string (this->m_mpv, "cursor-autohide", "no");
    mpv_set_option_string (this->m_mpv, "config", "no");
    mpv_set_option_string (this->m_mpv, "fbo-format", "rgba8");
    mpv_set_option_string (this->m_mpv, "vo", "libmpv");

    if (mpv_initialize (this->m_mpv) < 0)
        sLog.exception ("Could not initialize mpv context");

    mpv_set_option_string (this->m_mpv, "hwdec", "auto");
    mpv_set_option_string (this->m_mpv, "loop", "inf");
    mpv_set_option (this->m_mpv, "volume", MPV_FORMAT_DOUBLE, &volume);

    if (!this->getContext ().getApp ().getContext ().settings.audio.enabled) {
        mpv_set_option_string (this->m_mpv, "mute", "yes");
    }

    // initialize gl context for mpv
    mpv_opengl_init_params gl_init_params {get_proc_address, this};
    mpv_render_param params [] {{MPV_RENDER_PARAM_API_TYPE, const_cast<char*> (MPV_RENDER_API_TYPE_OPENGL)},
                                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
                                {MPV_RENDER_PARAM_INVALID, nullptr}};

    if (mpv_render_context_create (&this->m_mpvGl, this->m_mpv, params) < 0)
        sLog.exception ("Failed to initialize MPV's GL context");

    const std::filesystem::path videopath =
        this->getVideo ()->getProject ().getContainer ()->resolveRealFile (this->getVideo ()->getFilename ());

    // build the path to the video file
    const char* command [] = {"loadfile", videopath.c_str (), nullptr};

    if (mpv_command (this->m_mpv, command) < 0)
        sLog.exception ("Cannot load video to play");

    if (!this->getContext ().getApp ().getContext ().settings.audio.enabled) {
        const char* mutecommand [] = {"set", "mute", "yes", nullptr};

        mpv_command (this->m_mpv, mutecommand);
    }

    // setup framebuffers
    this->setupFramebuffers ();
}

void CVideo::setSize (int width, int height) {
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    // do not refresh the texture if any of the sizes are invalid
    if (this->m_width <= 0 || this->m_height <= 0)
        return;

    // reconfigure the texture
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, this->m_width, this->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void CVideo::renderFrame (glm::ivec4 viewport) {
    // read any and all the events available
    while (this->m_mpv) {
        const mpv_event* event = mpv_wait_event (this->m_mpv, 0);

        if (event == nullptr || event->event_id == MPV_EVENT_NONE)
            break;

        // we do not care about any of the events
        if (event->event_id == MPV_EVENT_VIDEO_RECONFIG) {
            int64_t width, height;

            if (mpv_get_property (this->m_mpv, "dwidth", MPV_FORMAT_INT64, &width) >= 0 &&
                mpv_get_property (this->m_mpv, "dheight", MPV_FORMAT_INT64, &height) >= 0)
                this->setSize (width, height);
        }
    }

    // render the next
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    mpv_opengl_fbo fbo {static_cast<int> (this->getWallpaperFramebuffer ()), static_cast<int> (this->m_width),
                        static_cast<int> (this->m_height), GL_RGBA8};

    // no need to flip as it'll be handled by the wallpaper rendering code
    int flip_y = 0;

    mpv_render_param params [] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo}, {MPV_RENDER_PARAM_FLIP_Y, &flip_y}, {MPV_RENDER_PARAM_INVALID, nullptr}};

    mpv_render_context_render (this->m_mpvGl, params);
}

Core::CVideo* CVideo::getVideo () {
    return this->getWallpaperData ()->as<Core::CVideo> ();
}

void CVideo::setPause (bool newState) {
    if (m_paused == newState)
        return;
    m_paused = newState;
    int pause = newState;
    mpv_set_property (m_mpv, "pause", MPV_FORMAT_FLAG, &pause);
}

int CVideo::getWidth () const {
    return this->m_width;
}

int CVideo::getHeight () const {
    return this->m_height;
}

const std::string CVideo::Type = "video";
