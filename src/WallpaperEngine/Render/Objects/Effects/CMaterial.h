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
        CMaterial (Irrlicht::CContext* context, Render::Objects::CImage* image, const Core::Objects::Images::CMaterial* material, irr::video::ITexture* texture);

        irr::video::ITexture* getOutputTexture () const;
        irr::video::ITexture* getInputTexture () const;

        const std::vector<CPass*>& getPasses () const;
        const CImage* getImage () const;

        void render ();

    private:
        void generatePasses ();
        void generateOutputMaterial ();

        Irrlicht::CContext* m_context;

        irr::video::ITexture* m_inputTexture;
        irr::video::ITexture* m_outputTexture;
        irr::video::SMaterial m_outputMaterial;

        Render::Objects::CImage* m_image;
        const Core::Objects::Images::CMaterial* m_material;

        std::vector<CPass*> m_passes;
    };
};
