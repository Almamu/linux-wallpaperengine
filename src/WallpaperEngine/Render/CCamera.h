#pragma once

#include "CScene.h"

#include "WallpaperEngine/Core/Scenes/CCamera.h"

namespace WallpaperEngine::Render
{
    class CScene;

    class CCamera
    {
    public:
        CCamera (CScene* scene, Core::Scenes::CCamera* camera);
        ~CCamera ();

        void setOrthogonalProjection (irr::u32 width, irr::u32 height);

        irr::core::vector3df* getCenter ();
        irr::core::vector3df* getEye ();
        irr::core::vector3df* getUp ();

    private:
        Core::Scenes::CCamera* m_camera;
        irr::scene::ICameraSceneNode* m_sceneCamera;
        CScene* m_scene;
    };
}
