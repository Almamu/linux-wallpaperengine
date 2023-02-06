#pragma once

#include <vector>

#include "WallpaperEngine/Audio/CAudioStream.h"

namespace WallpaperEngine::Audio
{
    class CAudioStream;
}

namespace WallpaperEngine::Audio::Drivers
{
    class CAudioDriver
    {
    public:
        virtual void addStream (CAudioStream* stream) = 0;

        virtual AVSampleFormat getFormat () const = 0;
        virtual int getSampleRate () const = 0;
        virtual int getChannels () const = 0;
    private:
    };
}