#pragma once

#include <vector>
#include <map>

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Audio/Drivers/CAudioDriver.h"

#include <SDL.h>

#define MAX_AUDIO_FRAME_SIZE 192000

namespace WallpaperEngine::Audio::Drivers
{
    /**
     * Audio output buffers for streams being played under SDL
     */
    struct CSDLAudioBuffer
    {
        CAudioStream* stream;
        uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2] = {0};
        unsigned int audio_buf_size = 0;
        unsigned int audio_buf_index = 0;
    };

    /**
     * SDL's audio driver implementation
     */
    class CSDLAudioDriver : public CAudioDriver
    {
    public:
        CSDLAudioDriver ();
        ~CSDLAudioDriver ();

        /** @inheritdoc */
        void addStream (CAudioStream* stream) override;
        /**
         * @return All the registered audio streams
         */
        const std::vector <CSDLAudioBuffer*>& getStreams ();

        /** @inheritdoc */
        [[nodiscard]] AVSampleFormat getFormat () const override;
        /** @inheritdoc */
        [[nodiscard]] int getSampleRate () const override;
        /** @inheritdoc */
        [[nodiscard]] int getChannels () const override;
        /**
         * @return The SDL's audio driver settings
         */
        [[nodiscard]] const SDL_AudioSpec& getSpec () const;

    private:
        /** The device's ID */
        SDL_AudioDeviceID m_deviceID;
        /** If the driver is initialized or not */
        bool m_initialized;
        /** The sound output configuration */
        SDL_AudioSpec m_audioSpec;
        /** All the playable steams */
        std::vector <CSDLAudioBuffer*> m_streams;
    };
}