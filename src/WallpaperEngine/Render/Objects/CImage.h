#pragma once

#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/CScene.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects
{
    class CImage : public CObject, public irr::video::IShaderConstantSetCallBack
    {
    public:
        CImage (CScene* scene, Core::Objects::CImage* image);

        virtual void OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData);

        void render () override;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;

    protected:
        static const std::string Type;

    private:
        void generateMaterial ();
        void generatePass (Core::Objects::Images::Materials::CPassess* pass);

        irr::video::S3DVertex m_vertex [4];
        irr::u32 m_passes;
        std::vector<irr::video::SMaterial> m_materials;
        std::vector<irr::video::ITexture*> m_renderTextures;

        Core::Objects::CImage* m_image;
        irr::core::aabbox3d<irr::f32> m_boundingBox;

        std::vector<Render::Shaders::Compiler*> m_vertexShaders;
        std::vector<Render::Shaders::Compiler*> m_pixelShaders;
    };
}
