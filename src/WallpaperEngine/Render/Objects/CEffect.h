#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Irrlicht/CContext.h"

#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/Effects/CFBO.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"

namespace WallpaperEngine::Render::Objects::Effects
{
    class CMaterial;
}

namespace WallpaperEngine::Render::Objects
{
    class CImage;

    class CEffect
    {
    public:
        CEffect (CImage* image, Core::Objects::CEffect* effect, Irrlicht::CContext* context, irr::video::ITexture* input);

        irr::video::ITexture *const getOutputTexture () const;
        irr::video::ITexture* getInputTexture () const;
        const CImage* getImage () const;

        const std::vector<Effects::CMaterial*>& getMaterials () const;

        Effects::CFBO* findFBO (const std::string& name);

        void render ();
    private:
        void generatePasses ();
        void generateFBOs ();
        void generateOutputMaterial ();

        Irrlicht::CContext* m_context;
        CImage* m_image;
        Core::Objects::CEffect* m_effect;

        std::vector<Effects::CFBO*> m_fbos;
        std::vector<Effects::CMaterial*> m_materials;

        irr::video::ITexture* m_inputTexture;
        irr::video::ITexture* m_outputTexture;
        irr::video::SMaterial m_outputMaterial;
    };
};
