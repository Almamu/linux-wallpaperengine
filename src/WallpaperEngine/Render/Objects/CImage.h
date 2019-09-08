#pragma once

#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/CScene.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects
{
    // TODO: MOVE IShaderConstantSetCallBack TO IT'S OWN CLASS OR ORGANIZE THIS BETTER
    class CImage : public CObject, public irr::video::IShaderConstantSetCallBack
    {
    public:
        CImage (CScene* scene, Core::Objects::CImage* image);

        virtual void OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData);

        void render () override;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;
        void OnRegisterSceneNode () override;

    protected:
        static const std::string Type;

    private:
        void generateMaterial ();

        irr::video::S3DVertex m_vertex [4];
        irr::video::SMaterial m_material;

        Core::Objects::CImage* m_image;
        irr::core::aabbox3d<irr::f32> m_boundingBox;

        Render::Shaders::Compiler* m_vertexShader;
        Render::Shaders::Compiler* m_pixelShader;
    };
}
