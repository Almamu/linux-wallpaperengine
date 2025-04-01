#pragma once

#include <string>

#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Render/Helpers/CContextAware.h"

#include "WallpaperEngine/Render/Wallpapers/CScene.h"

namespace WallpaperEngine::Render {
class CScene;

class CObject : public Helpers::CContextAware {
  public:
    template <class T> const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> bool is () {
        return this->m_type == T::Type;
    }

    virtual void render () = 0;

    [[nodiscard]] CScene* getScene () const;
    [[nodiscard]] CContainer* getContainer () const;
    [[nodiscard]] int getId () const;

  protected:
    CObject (CScene* scene, std::string type, Core::CObject* object);
    ~CObject () override = default;

  private:
    std::string m_type;

    CScene* m_scene;
    Core::CObject* m_object;
};
} // namespace WallpaperEngine::Render