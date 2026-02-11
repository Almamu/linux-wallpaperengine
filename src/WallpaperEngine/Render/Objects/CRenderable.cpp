#include "CRenderable.h"

#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Objects::Effects;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;

CRenderable::CRenderable (Wallpapers::CScene& scene, const Object& object) :
    CObject (scene, object),
    Render::FBOProvider (&scene) {
    TextureMap* textures = nullptr;

    if (object.is <Image> ()) {
        textures = &(*object.as <Image> ()->model->material->passes.begin ())->textures;
    } else if (object.is <Particle> ()) {
        textures = &(*object.as <Particle> ()->material->material->passes.begin ())->textures;
    }

    if (textures != nullptr && !textures->empty ()) {
        std::string textureName = textures->begin ()->second;

        if (textureName.find ("_rt_") == 0 || textureName.find ("_alias_") == 0) {
            this->m_texture = this->getScene ().findFBO (textureName);
        } else {
            this->m_texture = this->getContext ().resolveTexture (textureName);
        }
    }
}

void CRenderable::setup () {
    // calculate full animation time (if any)
    this->m_animationTime = 0.0f;

    for (const auto& cur : this->getTexture ()->getFrames ()) {
        this->m_animationTime += cur->frametime;
    }
}

std::shared_ptr<const TextureProvider> CRenderable::getTexture () const { return this->m_texture; }


double CRenderable::getAnimationTime () const { return this->m_animationTime; }