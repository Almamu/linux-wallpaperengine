#pragma once

#include <assert.h>

#include "CProject.h"

namespace WallpaperEngine::Core
{
    class CProject;

    class CWallpaper
    {
    public:
        template<class T> const T* as () const { assert (is <T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is <T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        CWallpaper (std::string type);

        CProject* getProject ();

    protected:
        friend class CProject;

        void setProject (CProject* project);

    private:
        CProject* m_project;

        std::string m_type;
    };
}
