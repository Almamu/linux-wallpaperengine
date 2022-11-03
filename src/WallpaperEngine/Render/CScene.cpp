#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CSound.h"

#include "CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CScene::CScene (Core::CScene* scene, CContext* context) :
    CWallpaper (scene, Type, context)
{
    // setup the scene camera
    this->m_camera = new CCamera (this, scene->getCamera ());

    // detect size if the orthogonal project is auto
    if (scene->getOrthogonalProjection ()->isAuto () == true)
    {
        // calculate the size of the projection based on the size of everything
        auto cur = scene->getObjects ().begin ();
        auto end = scene->getObjects ().end ();

        for (; cur != end; cur ++)
        {
            if ((*cur).second->is<Core::Objects::CImage> () == false)
                continue;

            glm::vec2 size = (*cur).second->as <Core::Objects::CImage> ()->getSize ();

            scene->getOrthogonalProjection ()->setWidth (size.x);
            scene->getOrthogonalProjection ()->setHeight (size.y);
        }
    }

    this->m_parallaxDisplacement = {0, 0};

    this->m_camera->setOrthogonalProjection (
        scene->getOrthogonalProjection ()->getWidth (),
        scene->getOrthogonalProjection ()->getHeight ()
    );

    // set clear color
    FloatColor clearColor = this->getScene ()->getClearColor ();

    glClearColor (clearColor.r, clearColor.g, clearColor.b, 1.0f);

    // setup framebuffers
    this->setupFramebuffers ();

    // create all objects based off their dependencies
    {
        auto cur = scene->getObjects ().begin ();
        auto end = scene->getObjects ().end ();

        for (; cur != end; cur++)
            this->createObject ((*cur).second);
    }

    // now setup the render order
    auto cur = scene->getObjectsByRenderOrder ().begin ();
    auto end = scene->getObjectsByRenderOrder ().end ();

    for (; cur != end; cur ++)
    {
        auto obj = this->m_objects.find ((*cur)->getId ());

        // ignores not created objects like particle systems
        if (obj == this->m_objects.end ())
            continue;

        this->m_objectsByRenderOrder.emplace_back ((*obj).second);
    }
}

Render::CObject* CScene::createObject (Core::CObject* object)
{
    Render::CObject* renderObject = nullptr;

    // ensure the item is not loaded already
    auto current = this->m_objects.find (object->getId ());

    if (current != this->m_objects.end ())
        return (*current).second;

    // check dependencies too!
    auto depCur = object->getDependencies ().begin ();
    auto depEnd = object->getDependencies ().end ();

    for (; depCur != depEnd; depCur ++)
    {
        // self-dependency is a possibility...
        if ((*depCur) == object->getId ())
            continue;

        auto dep = this->getScene ()->getObjects ().find (*depCur);

        if (dep != this->getScene ()->getObjects ().end ())
            this->createObject ((*dep).second);
    }

    if (object->is<Core::Objects::CImage>() == true)
    {
        Objects::CImage* image = new Objects::CImage (this, object->as<Core::Objects::CImage>());

        try
        {
            image->setup ();
        }
        catch (std::runtime_error ex)
        {
            std::cerr << "Cannot setup image resource: " << std::endl;
            std::cerr << ex.what () << std::endl;
        }

        renderObject = image;
    }
    else if (object->is<Core::Objects::CSound>() == true)
    {
        renderObject = new Objects::CSound (this, object->as<Core::Objects::CSound>());
    }

    if (renderObject != nullptr)
        this->m_objects.insert (std::make_pair (renderObject->getId (), renderObject));

    return renderObject;
}

CCamera* CScene::getCamera () const
{
    return this->m_camera;
}

void CScene::renderFrame (glm::ivec4 viewport)
{
    auto projection = this->getScene ()->getOrthogonalProjection ();
    auto cur = this->m_objectsByRenderOrder.begin ();
    auto end = this->m_objectsByRenderOrder.end ();

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);

    // update the parallax position if required
    if (this->getScene ()->isCameraParallax () == true)
    {
        float influence = this->getScene ()->getCameraParallaxMouseInfluence ();
        float amount = this->getScene ()->getCameraParallaxAmount ();
        float delay = this->getScene ()->getCameraParallaxDelay ();

        this->m_parallaxDisplacement.x = glm::mix (this->m_parallaxDisplacement.x, (this->m_mousePosition.x * amount) * influence, delay);
        this->m_parallaxDisplacement.y = glm::mix (this->m_parallaxDisplacement.y, (this->m_mousePosition.y * amount) * influence, delay);
    }

    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer());
    // ensure we render over the whole screen
    glViewport (0, 0, projection->getWidth (), projection->getHeight ());

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (; cur != end; cur ++)
        (*cur)->render ();
}

void CScene::updateMouse (glm::ivec4 viewport)
{
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


glm::vec2* CScene::getParallaxDisplacement ()
{
    return &this->m_parallaxDisplacement;
}

const std::string CScene::Type = "scene";
