#pragma once

#include <vector>

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Audio/CAudioStream.h"

namespace WallpaperEngine::Audio
{
    class CAudioStream;
}

namespace WallpaperEngine::Application
{
    class CApplicationContext;
}

namespace WallpaperEngine
{
    namespace Application
    {

    }

    namespace Audio
    {
        class CAudioStream;

        namespace Drivers
        {
            /**
             * Base class for audio driver implementations
             */
            class CAudioDriver
            {
            public:
                explicit CAudioDriver (Application::CApplicationContext& applicationContext);

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
                /**
                 * @return The application context under which the audio driver is initialized
                 */
                Application::CApplicationContext& getApplicationContext ();

            private:
                Application::CApplicationContext& m_applicationContext;
            };
        }
    }
}