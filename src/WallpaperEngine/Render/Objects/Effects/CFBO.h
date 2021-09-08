#pragma once

#include "WallpaperEngine/Core/Objects/Effects/CFBO.h"
#include "WallpaperEngine/Core/Objects/CImage.h"

namespace WallpaperEngine::Render::Objects::Effects
{
    class CFBO
    {
    public:
        CFBO (Core::Objects::Effects::CFBO* fbo, const Core::Objects::CImage* image);

        const std::string& getName () const;
        const float& getScale () const;
        const std::string& getFormat () const;

    private:
        const Core::Objects::Effects::CFBO* m_fbo;
    };
};
