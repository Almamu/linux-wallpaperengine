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

        /**
         * Registers the given stream in the driver for playing
         *
         * @param stream
         */
        void addStream (CAudioStream* stream);

        /**
         * TODO: MAYBE THIS SHOULD BE OUR OWN DEFINITIONS INSTEAD OF LIBRARY SPECIFIC ONES?
         *
         * @return The audio format the driver supports
         */
        [[nodiscard]] AVSampleFormat getFormat () const;
        /**
         * @return The sample rate the driver supports
         */
        [[nodiscard]] int getSampleRate () const;
        /**
         * @return The channels the driver supports
         */
        [[nodiscard]] int getChannels () const;

    private:
        /** The audio driver in use */
        Drivers::CAudioDriver& m_driver;
    };
}