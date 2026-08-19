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
    struct ResolvedTransform {
	glm::vec3 origin = glm::vec3 (0.0f);
	glm::vec3 scale = glm::vec3 (1.0f);
	float angle = 0.0f;
    };

    CObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~CObject () override = default;

    virtual void setup ();
    virtual void render ();

    [[nodiscard]] Wallpapers::CScene& getScene () const;
    [[nodiscard]] const AssetLocator& getAssetLocator () const;
    [[nodiscard]] int getId () const;
    [[nodiscard]] const Object& getObject () const;

    [[nodiscard]] ResolvedTransform resolveTransform () const;
    [[nodiscard]] ResolvedTransform resolveTransform (const WallpaperEngine::Data::Model::Object& object) const;

    /**
     * Computes the object's own transform (origin/scale/angle) without walking the
     * parent chain. Used as the per-node step of resolveTransform.
     */
    [[nodiscard]] static ResolvedTransform localTransform (const WallpaperEngine::Data::Model::Object& object);

private:
    Wallpapers::CScene& m_scene;
    const Object& m_object;
};
} // namespace WallpaperEngine::Render