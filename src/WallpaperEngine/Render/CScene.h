#pragma once

#include "CCamera.h"

#include "WallpaperEngine/Core/CScene.h"

#include "WallpaperEngine/Render/CWallpaper.h"

#include "WallpaperEngine/Irrlicht/CContext.h"

namespace WallpaperEngine::Render
{
    class CCamera;

    class CScene : public CWallpaper
    {
    public:
        CScene (Core::CScene* scene, WallpaperEngine::Irrlicht::CContext* context);

        CCamera* getCamera () const;
        int nextId ();

        void render () override;

        Core::CScene* getScene ();

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        CCamera* m_camera;
        irr::u32 m_nextId;
    };
}
