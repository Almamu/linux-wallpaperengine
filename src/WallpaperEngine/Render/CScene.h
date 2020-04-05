#pragma once

#include "CCamera.h"

#include "WallpaperEngine/Core/CScene.h"

#include "WallpaperEngine/Render/CWallpaper.h"

#include "WallpaperEngine/Irrlicht/CContext.h"

namespace WallpaperEngine::Irrlicht
{
    class CContext;
};

namespace WallpaperEngine::Render
{
    class CCamera;

    class CScene : public CWallpaper, public irr::scene::ISceneNode
    {
    public:
        CScene (Core::CScene* scene, Irrlicht::CContext* context);
        ~CScene () override;

        Irrlicht::CContext* getContext ();
        CCamera* getCamera () const;
        int nextId ();

        void render () override;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;
        void OnRegisterSceneNode () override;

        void renderWallpaper () override;

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        CCamera* m_camera;
        Irrlicht::CContext* m_context;
        irr::u32 m_nextId;
        irr::core::aabbox3d<irr::f32> m_boundingBox;
    };
}
