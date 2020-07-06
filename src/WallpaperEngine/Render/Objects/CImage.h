#pragma once

#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Render/Objects/CEffect.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/CScene.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects
{
    class CEffect;

    class CImage : public CObject, public irr::video::IShaderConstantSetCallBack
    {
    public:
        CImage (CScene* scene, Core::Objects::CImage* image);

        void OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData) override;

        void render () override;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;

        const Core::Objects::CImage* getImage () const;
        const std::vector<CEffect*>& getEffects () const;

    protected:
        static const std::string Type;

    private:
        void generateFBOs ();
        void generateMaterial ();
        void generatePass (Core::Objects::Images::Materials::CPass* pass);

        irr::video::S3DVertex m_vertex [4];
        irr::u32 m_passes;
        std::vector<irr::video::SMaterial> m_materials;
        std::vector<irr::video::ITexture*> m_renderTextures;

        Core::Objects::CImage* m_image;
        irr::core::aabbox3d<irr::f32> m_boundingBox;

        std::vector<CEffect*> m_effects;

        std::vector<Render::Shaders::Compiler*> m_vertexShaders;
        std::vector<Render::Shaders::Compiler*> m_pixelShaders;
    };
}
