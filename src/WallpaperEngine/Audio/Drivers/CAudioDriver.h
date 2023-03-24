#pragma once

#include <vector>

#include "WallpaperEngine/Audio/Detectors/CAudioPlayingDetector.h"
#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Audio/CAudioStream.h"

namespace WallpaperEngine
{
    namespace Application
    {
        class CApplicationContext;
    }

    namespace Audio
    {
        class CAudioStream;

        namespace Detectors
        {
            class CAudioPlayingDetector;
        }

        namespace Drivers
        {
            /**
             * Base class for audio driver implementations
             */
            class CAudioDriver
            {
            public:
                explicit CAudioDriver (Application::CApplicationContext& applicationContext, Detectors::CAudioPlayingDetector& detector);

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
                /**
                 * @return The audio playing detector to use to stop playing sound when something else starts playing
                 */
                [[nodiscard]] Detectors::CAudioPlayingDetector& getAudioDetector ();

            private:
                Application::CApplicationContext& m_applicationContext;
                Detectors::CAudioPlayingDetector& m_detector;
            };
        }
    }
}