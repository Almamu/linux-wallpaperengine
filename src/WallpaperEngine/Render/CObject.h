#pragma once

#include <string>

#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Render/Wallpapers/CScene.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render {
class CObject : public Helpers::ContextAware, public TypeCaster {
public:
    CObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~CObject () override = default;

    virtual void render ();

    [[nodiscard]] Wallpapers::CScene& getScene () const;
    [[nodiscard]] const AssetLocator& getAssetLocator () const;
    [[nodiscard]] int getId () const;
    [[nodiscard]] const Object& getObject () const;

private:
    Wallpapers::CScene& m_scene;
    const Object& m_object;
};
} // namespace WallpaperEngine::Render