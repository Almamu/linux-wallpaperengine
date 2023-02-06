#pragma once

#include <vector>
#include <map>

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Audio/Drivers/CAudioDriver.h"

#include <SDL.h>

#define MAX_AUDIO_FRAME_SIZE 192000

namespace WallpaperEngine::Audio::Drivers
{
    struct CSDLAudioBuffer
    {
        CAudioStream* stream;
        uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
        unsigned int audio_buf_size = 0;
        unsigned int audio_buf_index = 0;
    };

    class CSDLAudioDriver : public CAudioDriver
    {
    public:
        CSDLAudioDriver ();
        ~CSDLAudioDriver ();

        void addStream (CAudioStream* stream) override;
        const std::vector <CSDLAudioBuffer*>& getStreams ();

        AVSampleFormat getFormat () const override;
        int getSampleRate () const override;
        int getChannels () const override;
        const SDL_AudioSpec& getSpec () const;
    private:
        SDL_AudioDeviceID m_deviceID;
        bool m_initialized;
        SDL_AudioSpec m_audioSpec;
        std::vector <CSDLAudioBuffer*> m_streams;
    };
}