#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Irrlicht/CContext.h"

#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Shaders/Compiler.h"

namespace WallpaperEngine::Render::Objects::Effects
{
    class CMaterial;

    class CPass : public irr::video::IShaderConstantSetCallBack
    {
    public:
        CPass (Irrlicht::CContext* context, CMaterial* material, Core::Objects::Images::Materials::CPass* pass);

        void OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData) override;

        const irr::video::ITexture* getOutputTexture () const;
        const irr::video::ITexture* getInputTexture () const;

        const irr::video::SMaterial& getMaterial () const;

    private:
        void setupShaderVariables ();

        CMaterial* m_material;
        Core::Objects::Images::Materials::CPass* m_pass;
        Irrlicht::CContext* m_context;

        irr::video::ITexture* m_inputTexture;
        irr::video::ITexture* m_outputTexture;

        irr::video::SMaterial m_irrlichtMaterial;

        Render::Shaders::Compiler* m_fragShader;
        Render::Shaders::Compiler* m_vertShader;
    };
}
