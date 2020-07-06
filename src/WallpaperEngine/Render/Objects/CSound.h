#pragma once

#include <SDL_mixer.h>

#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/CScene.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects
{
    class CSound : public CObject
    {
    public:
        CSound (CScene* scene, Core::Objects::CSound* sound);

        void render () override {};
        const irr::core::aabbox3d<irr::f32>& getBoundingBox () const override;

    protected:
        static const std::string Type;

        void play ();

    private:
        Core::Objects::CSound* m_sound;
        irr::core::aabbox3d<irr::f32> m_boundingBox;

        std::vector <Mix_Music*> m_mixSdl;
        std::vector <SDL_RWops*> m_bufferReader;
        std::vector <void*> m_soundBuffer;
    };
}
