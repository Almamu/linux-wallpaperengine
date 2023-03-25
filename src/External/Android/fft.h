#pragma once

#include <iostream>

#define WAVE_BUFFER_SIZE 1024

namespace External::Android
{
    bool doFft (uint8_t* fft, uint8_t* waveform);
}