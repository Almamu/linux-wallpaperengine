#pragma once

namespace WallpaperEngine::Audio::Drivers::Recorders
{
    class CPlaybackRecorder
    {
    public:
        virtual void update () = 0;

        float audio16[16] = {0};
        float audio32[32] = {0};
        float audio64[64] = {0};
    };
}