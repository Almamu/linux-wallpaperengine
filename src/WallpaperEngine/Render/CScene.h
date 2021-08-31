#pragma once

#include "CCamera.h"

#include "WallpaperEngine/Core/CScene.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/CObject.h"

namespace WallpaperEngine::Render
{
    class CCamera;
    class CObject;

    class CScene : public CWallpaper
    {
    public:
        CScene (Core::CScene* scene, CContainer* container);

        CCamera* getCamera () const;

        void render () override;

        Core::CScene* getScene ();

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        CCamera* m_camera;
        std::vector<CObject*> m_objects;
    };
}
