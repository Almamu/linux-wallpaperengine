#include "CPulseAudioPlayingDetector.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <unistd.h>

namespace WallpaperEngine::Audio::Drivers::Detectors
{
    void sinkInputInfoCallback (pa_context* context, const pa_sink_input_info* info, int eol, void* userdata)
    {
        auto* detector = static_cast <CPulseAudioPlayingDetector*> (userdata);

        if (info == nullptr)
            return;

        if (info->proplist == nullptr)
            return;

        // get processid
        const char* value = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_PROCESS_ID);

        if (atoi (value) != getpid () && pa_cvolume_avg (&info->volume) != PA_VOLUME_MUTED)
            detector->setIsPlaying (true);
    }

    void defaultSinkInfoCallback (pa_context* context, const pa_server_info* info, void* userdata)
    {
        if (info == nullptr)
            return;

        pa_operation* op = pa_context_get_sink_input_info_list (context, sinkInputInfoCallback, userdata);

        pa_operation_unref (op);
    }

    CPulseAudioPlayingDetector::CPulseAudioPlayingDetector (
        Application::CApplicationContext& appContext,
        const Render::Drivers::Detectors::CFullScreenDetector& fullscreenDetector) :
        CAudioPlayingDetector (appContext, fullscreenDetector),
        m_mainloop (nullptr),
        m_mainloopApi (nullptr),
        m_context (nullptr)
    {
        this->m_mainloop = pa_mainloop_new ();
        this->m_mainloopApi = pa_mainloop_get_api (this->m_mainloop);
        this->m_context = pa_context_new (this->m_mainloopApi, "wallpaperengine");

        pa_context_connect (this->m_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

        // lock until pulseaudio allows connection
        while (pa_context_get_state (this->m_context) != PA_CONTEXT_READY)
            pa_mainloop_iterate (this->m_mainloop, 1, nullptr);
    }

    CPulseAudioPlayingDetector::~CPulseAudioPlayingDetector ()
    {
        if (this->m_context)
        {
            pa_context_disconnect (this->m_context);
            pa_context_unref (this->m_context);
        }

        if (this->m_mainloop)
            pa_mainloop_free (this->m_mainloop);
    }


    void CPulseAudioPlayingDetector::update ()
    {
        if (!this->getApplicationContext ().settings.audio.automute)
            return this->setIsPlaying (false);
        if (this->getFullscreenDetector ().anythingFullscreen ())
            return this->setIsPlaying (true);

        // reset playing state
        this->setIsPlaying (false);

        // start discovery of sinks
        pa_operation* op = pa_context_get_server_info (this->m_context, defaultSinkInfoCallback, (void*) this);

        // wait until all the operations are done
        while (pa_operation_get_state (op) == PA_OPERATION_RUNNING)
            pa_mainloop_iterate (this->m_mainloop, 1, nullptr);

        pa_operation_unref (op);
    }
}