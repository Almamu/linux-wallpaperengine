#pragma once

#include "CCamera.h"

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Core/CScene.h"

#include "WallpaperEngine/Irrlicht/CContext.h"

namespace WallpaperEngine::Render
{
    class CCamera;

    class CScene : public irr::scene::ISceneNode
    {
    public:
        CScene (const Core::CProject* project, Irrlicht::CContext* context);
        ~CScene () override;

        Irrlicht::CContext* getContext ();
        const Core::CScene* getScene () const;
        CCamera* getCamera () const;
        int nextId ();

        void render () override;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;
        void OnRegisterSceneNode () override;
    private:
        const Core::CProject* m_project;
        const Core::CScene* m_scene;
        CCamera* m_camera;
        Irrlicht::CContext* m_context;
        irr::u32 m_nextId;
        irr::core::aabbox3d<irr::f32> m_boundingBox;
    };
}
