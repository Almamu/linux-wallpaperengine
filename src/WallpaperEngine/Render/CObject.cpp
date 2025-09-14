#include "CObject.h"

#include <utility>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

CObject::CObject (Wallpapers::CScene& scene, const Object& object) :
    Helpers::ContextAware (scene),
    m_scene (scene),
    m_object (object) {}

Wallpapers::CScene& CObject::getScene () const {
    return this->m_scene;
}

const AssetLocator& CObject::getAssetLocator () const {
    return this->getScene ().getAssetLocator ();
}

int CObject::getId () const {
    return this->m_object.id;
}