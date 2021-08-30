#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Objects/Effects/CFBO.h"
#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Irrlicht/CContext.h"

namespace WallpaperEngine::Render::Objects::Effects
{
    class CFBO
    {
    public:
        CFBO (Core::Objects::Effects::CFBO* fbo, const Core::Objects::CImage* image);

        const irr::video::ITexture* getTexture () const;
        const std::string& getName () const;
        const irr::f32& getScale () const;
        const std::string& getFormat () const;
    private:
        irr::video::ITexture* m_texture;
        const Core::Objects::Effects::CFBO* m_fbo;
    };
};
