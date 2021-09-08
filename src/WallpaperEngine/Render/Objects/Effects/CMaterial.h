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
        CMaterial (const Render::Objects::CImage* image, const Core::Objects::Images::CMaterial* material);

        const std::vector<CPass*>& getPasses () const;
        const CImage* getImage () const;

        /**
         * Renders the given material, using inputTexture as first texture of the shader
         *
         * @param drawTo
         * @param inputTexture
         */
        void render (GLuint drawTo, GLuint inputTexture);

    private:
        void generatePasses ();

        const Render::Objects::CImage* m_image;
        const Core::Objects::Images::CMaterial* m_material;

        std::vector<CPass*> m_passes;
    };
};
