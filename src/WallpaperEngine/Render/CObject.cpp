#include "CObject.h"

#include <utility>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

CObject::CObject (Wallpapers::CScene* scene, std::string type, const Core::CObject* object) :
    Helpers::CContextAware (scene),
    m_type (std::move (type)),
    m_scene (scene),
    m_object (object) {}

Wallpapers::CScene* CObject::getScene () const {
    return this->m_scene;
}

const CContainer* CObject::getContainer () const {
    return this->getScene ()->getContainer ();
}

int CObject::getId () const {
    return this->m_object->getId ();
}