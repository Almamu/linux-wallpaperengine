#pragma once

#include <vector>

#include "WallpaperEngine/Audio/CAudioStream.h"

namespace WallpaperEngine::Audio
{
    class CAudioStream;
}

namespace WallpaperEngine::Audio::Drivers
{
    /**
     * Base class for audio driver implementations
     */
    class CAudioDriver
    {
    public:
        /**
         * Registers the given stream in the driver for playing
         *
         * @param stream
         */
        virtual void addStream (CAudioStream* stream) = 0;

        /**
         * TODO: MAYBE THIS SHOULD BE OUR OWN DEFINITIONS INSTEAD OF LIBRARY SPECIFIC ONES?
         *
         * @return The audio format the driver supports
         */
        [[nodiscard]] virtual AVSampleFormat getFormat () const = 0;
        /**
         * @return The sample rate the driver supports
         */
        [[nodiscard]] virtual int getSampleRate () const = 0;
        /**
         * @return The channels the driver supports
         */
        [[nodiscard]] virtual int getChannels () const = 0;
    };
}