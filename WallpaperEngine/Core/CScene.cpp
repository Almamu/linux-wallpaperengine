#include "CScene.h"
#include "CProject.h"

#include "Core.h"
#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Core;

CScene::CScene (
        Scenes::CCamera* camera,
        irr::video::SColorf ambientColor,
        bool bloom,
        irr::f64 bloomStrength,
        irr::f64 bloomThreshold,
        bool cameraFade,
        bool cameraParallax,
        irr::f64 cameraParallaxAmount,
        irr::f64 cameraParallaxDelay,
        irr::f64 cameraParallaxMouseInfluence,
        bool cameraPreview,
        bool cameraShake,
        irr::f64 cameraShakeAmplitude,
        irr::f64 cameraShakeRoughness,
        irr::f64 cameraShakeSpeed,
        irr::video::SColorf clearColor,
        Scenes::CProjection* orthogonalProjection,
        irr::video::SColorf skylightColor) :
    m_camera (camera),
    m_ambientColor (ambientColor),
    m_bloom (bloom),
    m_bloomStrength (bloomStrength),
    m_bloomThreshold (bloomThreshold),
    m_cameraFade (cameraFade),
    m_cameraParallax (cameraParallax),
    m_cameraParallaxAmount (cameraParallaxAmount),
    m_cameraParallaxDelay (cameraParallaxDelay),
    m_cameraParallaxMouseInfluence (cameraParallaxMouseInfluence),
    m_cameraPreview (cameraPreview),
    m_cameraShake (cameraShake),
    m_cameraShakeAmplitude (cameraShakeAmplitude),
    m_cameraShakeRoughness (cameraShakeRoughness),
    m_cameraShakeSpeed (cameraShakeSpeed),
    m_clearColor (clearColor),
    m_orthogonalProjection (orthogonalProjection),
    m_skylightColor (skylightColor)
{
}

CScene* CScene::fromFile (const irr::io::path& filename)
{
    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename));

    json::const_iterator camera_it = content.find ("camera");
    json::const_iterator general_it = content.find ("general");
    json::const_iterator objects_it = content.find ("objects");

    if (camera_it == content.end ())
    {
        throw std::runtime_error ("Scenes must have a defined camera");
    }

    if (general_it == content.end ())
    {
        throw std::runtime_error ("Scenes must have a general section");
    }

    if (objects_it == content.end ())
    {
        throw std::runtime_error ("Scenes must have a list of objects to display");
    }

    json::const_iterator ambientcolor_it = (*general_it).find ("ambientcolor");
    json::const_iterator bloom_it = (*general_it).find ("bloom");
    json::const_iterator bloomstrength_it = (*general_it).find ("bloomstrength");
    json::const_iterator bloomthreshold_it = (*general_it).find ("bloomthreshold");
    json::const_iterator camerafade_it = (*general_it).find ("camerafade");
    json::const_iterator cameraparallax_it = (*general_it).find ("cameraparallax");
    json::const_iterator cameraparallaxamount_it = (*general_it).find ("cameraparallaxamount");
    json::const_iterator cameraparallaxdelay_it = (*general_it).find ("cameraparallaxdelay");
    json::const_iterator cameraparallaxmouseinfluence_it = (*general_it).find ("cameraparallaxmouseinfluence");
    json::const_iterator camerapreview_it = (*general_it).find ("camerapreview");
    json::const_iterator camerashake_it = (*general_it).find ("camerashake");
    json::const_iterator camerashakeamplitude_it = (*general_it).find ("camerashakeamplitude");
    json::const_iterator camerashakeroughness_it = (*general_it).find ("camerashakeroughness");
    json::const_iterator camerashakespeed_it = (*general_it).find ("camerashakespeed");
    json::const_iterator clearcolor_it = (*general_it).find ("clearcolor");
    json::const_iterator orthogonalprojection_it = (*general_it).find ("orthogonalprojection");
    json::const_iterator skylightcolor_it = (*general_it).find ("skylightcolor");

    if (ambientcolor_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have ambient color");
    }

    if (bloom_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have bloom flag");
    }

    if (bloomstrength_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have bloom strength");
    }

    if (bloomthreshold_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have bloom threshold");
    }

    if (camerafade_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera fade");
    }

    if (cameraparallax_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera parallax");
    }

    if (cameraparallaxamount_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera parallax amount");
    }

    if (cameraparallaxdelay_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera parallax delay");
    }

    if (cameraparallaxmouseinfluence_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera parallax mouse influence");
    }

    if (camerapreview_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera preview");
    }

    if (camerashake_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera shake");
    }

    if (camerashakeamplitude_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera shake amplitude");
    }

    if (camerashakeroughness_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera shake roughness");
    }

    if (camerashakespeed_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have camera shake speed");
    }

    if (clearcolor_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have clear color");
    }

    if (orthogonalprojection_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have orthogonal projection info");
    }

    if (skylightcolor_it == (*general_it).end ())
    {
        throw std::runtime_error ("General section must have skylight color");
    }

    CScene* scene = new CScene (
            Scenes::CCamera::fromJSON (*camera_it),
            WallpaperEngine::Core::atoSColorf (*ambientcolor_it),
            *bloom_it,
            *bloomstrength_it,
            *bloomthreshold_it,
            *camerafade_it,
            *cameraparallax_it,
            *cameraparallaxamount_it,
            *cameraparallaxdelay_it,
            *cameraparallaxmouseinfluence_it,
            *camerapreview_it,
            *camerashake_it,
            *camerashakeamplitude_it,
            *camerashakeroughness_it,
            *camerashakespeed_it,
            WallpaperEngine::Core::atoSColorf (*clearcolor_it),
            Scenes::CProjection::fromJSON (*orthogonalprojection_it),
            WallpaperEngine::Core::atoSColorf (*skylightcolor_it)
    );

    json::const_iterator cur = (*objects_it).begin ();
    json::const_iterator end = (*objects_it).end ();

    for (; cur != end; cur ++)
    {
        scene->insertObject (
                CObject::fromJSON (*cur)
        );
    }

    return scene;
}


std::vector<CObject*>* CScene::getObjects ()
{
    return &this->m_objects;
}

void CScene::insertObject (CObject* object)
{
    this->m_objects.push_back (object);
}

CProject* CScene::getProject ()
{
    return this->m_project;
}

void CScene::setProject (CProject* project)
{
    this->m_project = project;
}

Scenes::CCamera* CScene::getCamera ()
{
    return this->m_camera;
}

const irr::video::SColorf &CScene::getAmbientColor() const
{
    return this->m_ambientColor;
}

bool CScene::isBloom () const
{
    return this->m_bloom;
}

irr::f64 CScene::getBloomStrength () const
{
    return this->m_bloomStrength;
}

irr::f64 CScene::getBloomThreshold () const
{
    return this->m_bloomThreshold;
}

bool CScene::isCameraFade () const
{
    return this->m_cameraFade;
}

bool CScene::isCameraParallax () const
{
    return this->m_cameraParallax;
}

irr::f64 CScene::getCameraParallaxAmount () const
{
    return this->m_cameraParallaxAmount;
}

irr::f64 CScene::getCameraParallaxDelay () const
{
    return this->m_cameraParallaxDelay;
}

irr::f64 CScene::getCameraParallaxMouseInfluence () const
{
    return this->m_cameraParallaxMouseInfluence;
}

bool CScene::isCameraPreview () const
{
    return this->m_cameraPreview;
}

bool CScene::isCameraShake () const
{
    return this->m_cameraShake;
}

irr::f64 CScene::getCameraShakeAmplitude () const
{
    return this->m_cameraShakeAmplitude;
}

irr::f64 CScene::getCameraShakeRoughness () const
{
    return this->m_cameraShakeRoughness;
}

irr::f64 CScene::getCameraShakeSpeed () const
{
    return this->m_cameraShakeSpeed;
}

const irr::video::SColorf &CScene::getClearColor () const
{
    return this->m_clearColor;
}

Scenes::CProjection* CScene::getOrthogonalProjection () const
{
    return this->m_orthogonalProjection;
}

const irr::video::SColorf &CScene::getSkylightColor () const
{
    return this->m_skylightColor;
}
