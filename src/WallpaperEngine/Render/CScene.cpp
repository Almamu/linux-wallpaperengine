#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CSound.h"

#include "CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CScene::CScene (Core::CScene* scene, CContainer* container, CContext* context) :
    CWallpaper (scene, Type, container, context)
{
    // setup the scene camera
    this->m_camera = new CCamera (this, scene->getCamera ());
    this->m_camera->setOrthogonalProjection (
        scene->getOrthogonalProjection ()->getWidth (),
        scene->getOrthogonalProjection ()->getHeight ()
    );
    // setup framebuffers
    this->setupFramebuffers ();

    auto cur = scene->getObjects ().begin ();
    auto end = scene->getObjects ().end ();

    for (; cur != end; cur ++)
    {
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

    auto objectsCur = this->m_objects.begin ();
    auto objectsEnd = this->m_objects.end ();

    for (; objectsCur != objectsEnd; objectsCur ++)
    {
        if ((*objectsCur)->is <Objects::CImage> () == true)
            (*objectsCur)->as <Objects::CImage> ()->setup ();
    }
}

CCamera* CScene::getCamera () const
{
    return this->m_camera;
}

void CScene::renderFrame (glm::ivec4 viewport)
{
    auto projection = this->getScene ()->getOrthogonalProjection ();
    auto cur = this->m_objects.begin ();
    auto end = this->m_objects.end ();

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);

    // clear screen
    FloatColor clearColor = this->getScene ()->getClearColor ();

    glClearColor (clearColor.r, clearColor.g, clearColor.b, 0.0f);

    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer());
    // ensure we render over the whole screen
    glViewport (0, 0, projection->getWidth (), projection->getHeight ());

    for (; cur != end; cur ++)
        (*cur)->render ();

    // ensure we render over the whole screen
    glViewport (0, 0, projection->getWidth (), projection->getHeight ());
}

void CScene::updateMouse (glm::ivec4 viewport)
{
    // projection also affects the mouse position
    auto projection = this->getScene ()->getOrthogonalProjection ();
    // update virtual mouse position first
    CMouseInput* mouse = this->getContext ()->getMouse ();
    // TODO: PROPERLY TRANSLATE THESE TO WHAT'S VISIBLE ON SCREEN (FOR BACKGROUNDS THAT DO NOT EXACTLY FIT ON SCREEN)

    this->m_mousePosition.x = glm::clamp ((mouse->position.x - viewport.x) / viewport.z, 0.0, 1.0);
    this->m_mousePosition.y = glm::clamp ((mouse->position.y - viewport.y) / viewport.w, 0.0, 1.0);

    // screen-space positions have to be transposed to what the screen will actually show
}

Core::CScene* CScene::getScene ()
{
    return this->getWallpaperData ()->as<Core::CScene> ();
}

glm::vec2* CScene::getMousePosition ()
{
    return &this->m_mousePosition;
}

const std::string CScene::Type = "scene";
