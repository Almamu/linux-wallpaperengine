#pragma once

#include <cassert>
#include <string>

#include "CProject.h"

namespace WallpaperEngine::Core {
class CProject;

class CWallpaper {
  public:
    template <class T> [[nodiscard]] const T* as () const {
        if (is<T> ()) {
            return static_cast <const T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* as () {
        if (is<T> ()) {
            return static_cast <T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] bool is () const {
        return typeid(*this) == typeid(T);
    }

    std::shared_ptr <const CProject> getProject () const;

  protected:
    friend class CProject;

    explicit CWallpaper (std::shared_ptr <const CProject> project);
    virtual ~CWallpaper() = default;

  private:
    std::shared_ptr <const CProject> m_project;
};
} // namespace WallpaperEngine::Core
