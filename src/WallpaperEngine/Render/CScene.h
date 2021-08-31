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
        int nextId ();

        void render () override;

        Core::CScene* getScene ();

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        CCamera* m_camera;
        uint32_t m_nextId;
        std::vector<CObject*> m_objects;
    };
}
