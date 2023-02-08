#include "common.h"
#include "CVideo.h"

#include <pthread.h>
#include <GL/glew.h>

extern bool g_AudioEnabled;
extern int g_AudioVolume;

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

void* get_proc_address (void* ctx, const char* name)
{
    return reinterpret_cast <void*> (glfwGetProcAddress (name));
}

CVideo::CVideo (Core::CVideo* video, CRenderContext& context, CAudioContext& audioContext) :
    CWallpaper (video, Type, context, audioContext),
    m_width (16),
    m_height (16)
{
    double volume = g_AudioVolume * 100.0 / 128.0;

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

    if (mpv_initialize (this->m_mpv) < 0)
        sLog.exception ("Could not initialize mpv context");

    mpv_set_option_string (this->m_mpv, "hwdec", "auto");
    mpv_set_option_string (this->m_mpv, "loop", "inf");
    mpv_set_option (this->m_mpv, "volume", MPV_FORMAT_DOUBLE, &volume);

    // initialize gl context for mpv
    mpv_opengl_init_params gl_init_params {get_proc_address, nullptr};
    mpv_render_param params[] {
        {MPV_RENDER_PARAM_API_TYPE, const_cast <char*> (MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if (mpv_render_context_create (&this->m_mpvGl, this->m_mpv, params) < 0)
        sLog.exception ("Failed to initialize MPV's GL context");

    const char* command [] = {
        "loadfile", this->getVideo ()->getFilename ().c_str (), nullptr
    };

    if (mpv_command (this->m_mpv, command) < 0)
        sLog.exception ("Cannot load video to play");

    if (g_AudioEnabled == false)
    {
        const char* mutecommand [] = {
            "set", "mute", "yes", nullptr
        };

        mpv_command (this->m_mpv, mutecommand);
    }

    // setup framebuffers
    this->setupFramebuffers();
}

void CVideo::setSize (int64_t width, int64_t height)
{
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    // do not refresh the texture if any of the sizes are invalid
    if (this->m_width <= 0 || this->m_height <= 0)
        return;

    // reconfigure the texture
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture());
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, this->m_width, this->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void CVideo::renderFrame (glm::ivec4 viewport)
{
    // read any and all the events available
    while (this->m_mpv)
    {
        mpv_event* event = mpv_wait_event (this->m_mpv, 0);

        if (event == nullptr || event->event_id == MPV_EVENT_NONE)
            break;

        // we do not care about any of the events
        switch (event->event_id)
        {
            case MPV_EVENT_VIDEO_RECONFIG:
                {
                    int64_t width, height;

                    if (mpv_get_property (this->m_mpv, "dwidth", MPV_FORMAT_INT64, &width) >= 0 &&
                        mpv_get_property (this->m_mpv, "dheight", MPV_FORMAT_INT64, &height) >= 0)
                        this->setSize (width, height);
                }
                break;
        }
    }

    // render the next
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    mpv_opengl_fbo fbo {
        static_cast <int> (this->getWallpaperFramebuffer()),
        static_cast <int> (this->m_width),
        static_cast <int> (this->m_height),
        GL_RGBA8
    };

    // no need to flip as it'll be handled by the wallpaper rendering code
    int flip_y = 0;

    mpv_render_param params [] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_context_render (this->m_mpvGl, params);
}

Core::CVideo* CVideo::getVideo ()
{
    return this->getWallpaperData ()->as<Core::CVideo> ();
}

uint32_t CVideo::getWidth () const
{
    return this->m_width;
}

uint32_t CVideo::getHeight () const
{
    return this->m_height;
}

const std::string CVideo::Type = "video";
