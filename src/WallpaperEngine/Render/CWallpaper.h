#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/CWallpaper.h"
#include "WallpaperEngine/Core/CScene.h"
#include "WallpaperEngine/Core/CVideo.h"

#include "WallpaperEngine/Irrlicht/CContext.h"

namespace WallpaperEngine::Irrlicht
{
    class CContext;
};

namespace WallpaperEngine::Render
{
    class CWallpaper : public irr::scene::ISceneNode
    {
    public:
        template<class T> const T* as () const { assert (is<T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is<T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        CWallpaper (Core::CWallpaper* wallpaperData, std::string type, WallpaperEngine::Irrlicht::CContext* context);
        ~CWallpaper () override;

        void OnRegisterSceneNode () override;

        WallpaperEngine::Irrlicht::CContext* getContext () const;
        const irr::core::aabbox3d<irr::f32>& getBoundingBox () const override;

    protected:
        WallpaperEngine::Irrlicht::CContext* m_context;
        Core::CWallpaper* m_wallpaperData;

        Core::CWallpaper* getWallpaperData ();

    private:
        irr::core::aabbox3d<irr::f32> m_boundingBox = irr::core::aabbox3d<irr::f32> (0, 0, 0, 0, 0, 0);

        std::string m_type;
    };
}
