#pragma once

#include <SDL_rwops.h>
#include <SDL_mixer.h>

#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/CObject.h"

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
        void play ();

    private:
        std::vector<std::string> m_filenames;
        std::vector <Mix_Music*> m_sdl;
        std::vector <SDL_RWops*> m_bufferReader;
        std::vector <void*> m_soundBuffer;

        Core::Objects::CSound* m_sound;
    };
}
