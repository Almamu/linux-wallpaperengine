#include "CSDLAudioDriver.h"
#include "WallpaperEngine/Logging/CLog.h"

#define SDL_AUDIO_BUFFER_SIZE 4096
#define MAX_AUDIO_FRAME_SIZE 192000

using namespace WallpaperEngine::Audio;
using namespace WallpaperEngine::Audio::Drivers;

void audio_callback (void* userdata, uint8_t* streamData, int length) {
    auto* driver = static_cast<CSDLAudioDriver*> (userdata);

    memset (streamData, 0, length);

    // if audio is playing do not do anything here!
    if (driver->getAudioDetector ().anythingPlaying ())
        return;

    for (const auto& buffer : driver->getStreams ()) {
        uint8_t* streamDataPointer = streamData;
        int streamLength = length;

        // sound is not initialized or stopped and is not in loop mode
        // ignore mixing it in
        if (!buffer->stream->isInitialized ())
            continue;

        // check if queue is empty and signal the read thread
        if (buffer->stream->isQueueEmpty ()) {
            SDL_CondSignal (buffer->stream->getWaitCondition ());
            continue;
        }

        while (streamLength > 0 && driver->getApplicationContext ().state.general.keepRunning) {
            if (buffer->audio_buf_index >= buffer->audio_buf_size) {
                // get more data to fill the buffer
                int audio_size = buffer->stream->decodeFrame (buffer->audio_buf, sizeof (buffer->audio_buf));

                if (audio_size < 0) {
                    // fallback for errors, silence
                    buffer->audio_buf_size = 1024;
                    memset (buffer->audio_buf, 0, buffer->audio_buf_size);
                } else {
                    buffer->audio_buf_size = audio_size;
                }

                buffer->audio_buf_index = 0;
            }

            int len1 = buffer->audio_buf_size - buffer->audio_buf_index;

            if (len1 > streamLength)
                len1 = streamLength;

            // mix the audio
            SDL_MixAudioFormat (
                streamDataPointer, &buffer->audio_buf [buffer->audio_buf_index], driver->getSpec ().format,
                len1, driver->getApplicationContext ().state.audio.volume);

            streamLength -= len1;
            streamDataPointer += len1;
            buffer->audio_buf_index += len1;
        }
    }
}

CSDLAudioDriver::CSDLAudioDriver (
    Application::CApplicationContext& applicationContext, Detectors::CAudioPlayingDetector& detector,
    Recorders::CPlaybackRecorder& recorder
) :
    CAudioDriver (applicationContext, detector, recorder),
    m_initialized (false),
    m_audioSpec () {
    if (SDL_InitSubSystem (SDL_INIT_AUDIO) < 0) {
        sLog.error ("Cannot initialize SDL audio system, SDL_GetError: ", SDL_GetError ());
        sLog.error ("Continuing without audio support");

        return;
    }

    const SDL_AudioSpec requestedSpec = {
        .freq = 48000,
        .format = AUDIO_F32,
        .channels = 2,
        .samples = SDL_AUDIO_BUFFER_SIZE,
        .callback = audio_callback,
        .userdata = this
    };

    this->m_deviceID =
        SDL_OpenAudioDevice (nullptr, false, &requestedSpec, &this->m_audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (this->m_deviceID == 0) {
        sLog.error ("SDL_OpenAudioDevice: ", SDL_GetError ());
        return;
    }

    SDL_PauseAudioDevice (this->m_deviceID, 0);

    this->m_initialized = true;
}

CSDLAudioDriver::~CSDLAudioDriver () {
    if (!this->m_initialized)
        return;

    SDL_CloseAudioDevice (this->m_deviceID);
    SDL_QuitSubSystem (SDL_INIT_AUDIO);
}

void CSDLAudioDriver::addStream (CAudioStream* stream) {
    this->m_streams.push_back (new CSDLAudioBuffer {stream});
}

const std::vector<CSDLAudioBuffer*>& CSDLAudioDriver::getStreams () {
    return this->m_streams;
}

AVSampleFormat CSDLAudioDriver::getFormat () const {
    switch (this->m_audioSpec.format) {
        case AUDIO_U8:
        case AUDIO_S8: return AV_SAMPLE_FMT_U8;
        case AUDIO_U16MSB:
        case AUDIO_U16LSB:
        case AUDIO_S16LSB:
        case AUDIO_S16MSB: return AV_SAMPLE_FMT_S16;
        case AUDIO_S32LSB:
        case AUDIO_S32MSB: return AV_SAMPLE_FMT_S32;
        case AUDIO_F32LSB:
        case AUDIO_F32MSB: return AV_SAMPLE_FMT_FLT;
    }

    sLog.exception ("Cannot convert from SDL format to ffmpeg format, aborting...");
}

int CSDLAudioDriver::getSampleRate () const {
    return this->m_audioSpec.freq;
}

int CSDLAudioDriver::getChannels () const {
    return this->m_audioSpec.channels;
}

const SDL_AudioSpec& CSDLAudioDriver::getSpec () const {
    return this->m_audioSpec;
}