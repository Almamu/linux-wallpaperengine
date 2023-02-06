#include "common.h"
#include "CSDLAudioDriver.h"

#define SDL_AUDIO_BUFFER_SIZE 4096
#define MAX_AUDIO_FRAME_SIZE 192000

extern int g_AudioVolume;
extern bool g_KeepRunning;

using namespace WallpaperEngine::Audio;
using namespace WallpaperEngine::Audio::Drivers;

void audio_callback (void* userdata, uint8_t* streamData, int length)
{
    CSDLAudioDriver* driver = reinterpret_cast <CSDLAudioDriver*> (userdata);

    memset (streamData, 0, length);

    for (const auto& buffer : driver->getStreams ())
    {
        uint8_t* streamDataPointer = streamData;
        int streamLength = length;
        int len1, audio_size;

        // sound is not initialized or stopped and is not in loop mode
        // ignore mixing it in
        if (buffer->stream->isInitialized () == false)
            continue;

        // check if thread is empty and signal the read thread
        if (buffer->stream->isQueueEmpty () == true)
        {
            SDL_CondSignal (buffer->stream->getWaitCondition ());
            continue;
        }

        while (streamLength > 0 && g_KeepRunning)
        {
            if (buffer->audio_buf_index >= buffer->audio_buf_size)
            {
                // get more data to fill the buffer
                audio_size = buffer->stream->decodeFrame (buffer->audio_buf, sizeof (buffer->audio_buf));

                if (audio_size < 0)
                {
                    // fallback for errors, silence
                    buffer->audio_buf_size = 1024;
                    memset(buffer->audio_buf, 0, buffer->audio_buf_size);
                }
                else
                {
                    buffer->audio_buf_size = audio_size;
                }

                buffer->audio_buf_index = 0;
            }

            len1 = buffer->audio_buf_size - buffer->audio_buf_index;

            if (len1 > streamLength)
                len1 = streamLength;

            // mix the audio
            SDL_MixAudioFormat (
                streamDataPointer, &buffer->audio_buf [buffer->audio_buf_index],
                driver->getSpec ().format, len1, g_AudioVolume
            );

            streamLength -= len1;
            streamDataPointer += len1;
            buffer->audio_buf_index += len1;
        }
    }
}

CSDLAudioDriver::CSDLAudioDriver () :
    m_initialized (false)
{
    if (SDL_InitSubSystem (SDL_INIT_AUDIO) < 0)
    {
        sLog.error ("Cannot initialize SDL audio system, SDL_GetError: ", SDL_GetError ());
        sLog.error ("Continuing without audio support");

        return;
    }

    SDL_AudioSpec requestedSpec;

    memset (&requestedSpec, 0, sizeof (requestedSpec));

    requestedSpec.freq = 48000;
    requestedSpec.format = AUDIO_F32;
    requestedSpec.channels = 2;
    requestedSpec.samples = SDL_AUDIO_BUFFER_SIZE;
    requestedSpec.callback = audio_callback;
    requestedSpec.userdata = this;

    this->m_deviceID = SDL_OpenAudioDevice (nullptr, false, &requestedSpec, &this->m_audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (this->m_deviceID == 0)
    {
        sLog.error ("SDL_OpenAudioDevice: ", SDL_GetError ());
        return;
    }

    SDL_PauseAudioDevice (this->m_deviceID, 0);

    this->m_initialized = true;
}

CSDLAudioDriver::~CSDLAudioDriver ()
{
    if (this->m_initialized == false)
        return;

    SDL_CloseAudioDevice (this->m_deviceID);
    SDL_QuitSubSystem (SDL_INIT_AUDIO);
}

void CSDLAudioDriver::addStream (CAudioStream* stream)
{
    this->m_streams.push_back (new CSDLAudioBuffer { stream });
}
const std::vector <CSDLAudioBuffer*>& CSDLAudioDriver::getStreams ()
{
    return this->m_streams;
}

AVSampleFormat CSDLAudioDriver::getFormat () const
{
    switch (this->m_audioSpec.format)
    {
        case AUDIO_U8: case AUDIO_S8: return AV_SAMPLE_FMT_U8;
        case AUDIO_U16MSB: case AUDIO_U16LSB: case AUDIO_S16LSB: case AUDIO_S16MSB: return AV_SAMPLE_FMT_S16;
        case AUDIO_S32LSB: case AUDIO_S32MSB: return AV_SAMPLE_FMT_S32;
        case AUDIO_F32LSB: case AUDIO_F32MSB: return AV_SAMPLE_FMT_FLT;
    }

    sLog.exception ("Cannot convert from SDL format to ffmpeg format, aborting...");
}

int CSDLAudioDriver::getSampleRate () const
{
    return this->m_audioSpec.freq;
}

int CSDLAudioDriver::getChannels () const
{
    return this->m_audioSpec.channels;
}
const SDL_AudioSpec& CSDLAudioDriver::getSpec () const
{
    return this->m_audioSpec;
}