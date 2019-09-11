#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"
#include "WallpaperEngine/Core/Objects/CEffect.h"

#include "WallpaperEngine/Irrlicht/CContext.h"

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
        CMaterial (Irrlicht::CContext* context, Render::Objects::CEffect* effect, Core::Objects::Images::CMaterial* material);

        const irr::video::ITexture* getOutputTexture () const;
        const irr::video::ITexture* getInputTexture () const;

        const std::vector<CPass*>& getPasses () const;
        const CImage* getImage () const;

    private:
        void generatePasses ();

        Irrlicht::CContext* m_context;

        irr::video::ITexture* m_inputTexture;
        irr::video::ITexture* m_outputTexture;

        Render::Objects::CEffect* m_effect;
        Core::Objects::Images::CMaterial* m_material;

        std::vector<CPass*> m_passes;
    };
};
