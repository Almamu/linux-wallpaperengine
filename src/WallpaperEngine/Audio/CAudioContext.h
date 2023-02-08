#pragma once

#include <libavutil/samplefmt.h>
#include <vector>

namespace WallpaperEngine::Audio::Drivers
{
    class CAudioDriver;
}

namespace WallpaperEngine::Audio
{
    class CAudioStream;

    class CAudioContext
    {
    public:
        explicit CAudioContext (Drivers::CAudioDriver& driver);

        void addStream (CAudioStream* stream);

        AVSampleFormat getFormat () const;
        int getSampleRate () const;
        int getChannels () const;
    private:
        Drivers::CAudioDriver& m_driver;
    };
}