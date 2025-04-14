#pragma once

#include <cassert>
#include <string>

#include "CProject.h"

namespace WallpaperEngine::Core {
class CProject;

class CWallpaper {
  public:
    template <class T> const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> bool is () const {
        return this->m_type == T::Type;
    }

    CWallpaper (std::string type, const CProject& project);

    const CProject& getProject () const;

  protected:
    friend class CProject;

  private:
    const CProject& m_project;

    const std::string m_type;
};
} // namespace WallpaperEngine::Core
