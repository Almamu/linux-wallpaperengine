#pragma once

#include <string>

#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Render/Wallpapers/CScene.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render {
class CObject : public Helpers::ContextAware {
public:
    template <class T> [[nodiscard]] const T* as () const {
	if (is<T> ()) {
	    return static_cast<const T*> (this);
	}

	throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* as () {
	if (is<T> ()) {
	    return static_cast<T*> (this);
	}

	throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] bool is () const { return typeid (*this) == typeid (T); }

    virtual void render () = 0;

    [[nodiscard]] Wallpapers::CScene& getScene () const;
    [[nodiscard]] const AssetLocator& getAssetLocator () const;
    [[nodiscard]] int getId () const;

    virtual ~CObject () override = default;

protected:
    CObject (Wallpapers::CScene& scene, const Object& object);

private:
    Wallpapers::CScene& m_scene;
    const Object& m_object;
};
} // namespace WallpaperEngine::Render