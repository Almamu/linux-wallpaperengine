#pragma once

#include "CScene.h"

#include "WallpaperEngine/Core/Scenes/CCamera.h"

namespace WallpaperEngine::Render
{
    class CScene;

    class CCamera
    {
    public:
        CCamera (CScene* scene, const Core::Scenes::CCamera* camera);
        ~CCamera ();

        void setOrthogonalProjection (irr::f32 width, irr::f32 height);

        const irr::core::vector3df& getCenter () const;
        const irr::core::vector3df& getEye () const;
        const irr::core::vector3df& getUp () const;

    private:
        const Core::Scenes::CCamera* m_camera;
        irr::scene::ICameraSceneNode* m_sceneCamera;
        CScene* m_scene;
    };
}
