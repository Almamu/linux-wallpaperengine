#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CSound.h"

#include "CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CScene::CScene (Core::CScene* scene, CContainer* container) :
    CWallpaper (scene, Type, container)
{
    // setup the scene camera
    this->m_camera = new CCamera (this, scene->getCamera ());
    this->m_camera->setOrthogonalProjection (
        scene->getOrthogonalProjection ()->getWidth (),
        scene->getOrthogonalProjection ()->getHeight ()
    );

    auto cur = scene->getObjects ().begin ();
    auto end = scene->getObjects ().end ();

    int highestId = 0;

    for (; cur != end; cur ++)
    {
        if ((*cur)->getId () > highestId)
            highestId = (*cur)->getId ();

        CObject* object = nullptr;

        if ((*cur)->is<Core::Objects::CImage>() == true)
        {
            object = new Objects::CImage (this, (*cur)->as<Core::Objects::CImage>());
        }
        else if ((*cur)->is<Core::Objects::CSound>() == true)
        {
            object = new Objects::CSound (this, (*cur)->as<Core::Objects::CSound>());
        }

        if (object != nullptr)
            this->m_objects.emplace_back (object);
    }
}

CCamera* CScene::getCamera () const
{
    return this->m_camera;
}

int CScene::nextId ()
{
    return this->m_nextId ++;
}

void CScene::render ()
{
    auto cur = this->m_objects.begin ();
    auto end = this->m_objects.end ();

    for (; cur != end; cur ++)
        (*cur)->render ();
}

Core::CScene* CScene::getScene ()
{
    return this->getWallpaperData ()->as<Core::CScene> ();
}

const std::string CScene::Type = "scene";
