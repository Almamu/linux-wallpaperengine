#pragma once

#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"
#include "WallpaperEngine/Core/Objects/CEffect.h"

#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Objects/CEffect.h"

#include "CPass.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects
{
    class CEffect;
    class CImage;
}

namespace WallpaperEngine::Render::Objects::Effects
{
    class CPass;

    class CMaterial
    {
        friend class CPass;
    public:
        CMaterial (const Render::Objects::CEffect* effect, const Core::Objects::Images::CMaterial* material);

        const std::vector<CPass*>& getPasses () const;
        CImage* getImage () const;
        const Core::Objects::Images::CMaterial* getMaterial () const;

    private:
        void generatePasses ();

        const Render::Objects::CEffect* m_effect;
        const Core::Objects::Images::CMaterial* m_material;

        std::vector<CPass*> m_passes;
    };
};
