#pragma once

#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Objects/CEffect.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/CScene.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects::Effects
{
    class CMaterial;
}

namespace WallpaperEngine::Render::Objects
{
    class CEffect;

    class CImage : public CObject
    {
    public:
        CImage (CScene* scene, Core::Objects::CImage* image);

        void render () override;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;

        const Core::Objects::CImage* getImage () const;
        const std::vector<CEffect*>& getEffects () const;

        const irr::video::S3DVertex* getVertex () const;

    protected:
        static const std::string Type;

    private:
        void generateMaterial (irr::video::ITexture* resultTexture);

        irr::video::S3DVertex m_vertex [4];

        Core::Objects::CImage* m_image;
        irr::core::aabbox3d<irr::f32> m_boundingBox;

        std::vector<CEffect*> m_effects;
        Effects::CMaterial* m_material;
        irr::video::SMaterial m_irrlichtMaterial;
    };
}
