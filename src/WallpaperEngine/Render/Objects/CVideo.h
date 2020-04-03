#pragma once

#include "WallpaperEngine/Core/Objects/CVideo.h"

#include "WallpaperEngine/Render/CScene.h"
#include "WallpaperEngine/Render/CObject.h"

namespace WallpaperEngine::Render::Objects
{
    class CVideo : public CObject
    {
    public:
        CVideo (CScene* scene, Core::Objects::CVideo* video);

        void render () override;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox () const override;

    protected:
        static const std::string Type;

    private:
        irr::video::IImage* m_frameImage;
        irr::video::ITexture* m_frameTexture;

        Core::Objects::CVideo* m_video;
        irr::core::aabbox3d<irr::f32> m_boundingBox;
    };
};
