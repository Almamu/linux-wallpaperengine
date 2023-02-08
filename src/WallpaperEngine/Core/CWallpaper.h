#pragma once

#include <cassert>

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

        CWallpaper (std::string type, CProject& project);

        CProject& getProject () const;

    protected:
        friend class CProject;

    private:
        CProject& m_project;

        std::string m_type;
    };
}
