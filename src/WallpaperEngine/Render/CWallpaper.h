#pragma once

#include <assert.h>

#include "WallpaperEngine/Core/CWallpaper.h"

namespace WallpaperEngine::Render
{
    class CWallpaper
    {
    public:
        template<class T> const T* as () const { assert (is<T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is<T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        CWallpaper (Core::CWallpaper* wallpaperData, std::string type);

        Core::CWallpaper* getWallpaperData ();

        virtual void renderWallpaper () = 0;
    
    protected:
        Core::CWallpaper* m_wallpaperData;

    private:
        std::string m_type;
    };
}
