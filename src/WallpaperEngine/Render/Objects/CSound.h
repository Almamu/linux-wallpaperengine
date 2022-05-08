#pragma once

#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Audio/CAudioStream.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects
{
    class CSound : public CObject
    {
    public:
        CSound (CScene* scene, Core::Objects::CSound* sound);

        void render () override;

    protected:
        static const std::string Type;

        void load ();

    private:
        std::vector <void*> m_soundBuffer;
        std::vector <Audio::CAudioStream*> m_audioStreams;

        Core::Objects::CSound* m_sound;
    };
}
