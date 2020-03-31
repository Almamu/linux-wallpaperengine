#include "CScene.h"
#include "CProject.h"

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

CScene* CScene::fromFile (const irr::io::path& filename, const char *type)
{
    if (strcmp(type, "scene") == 0)
    {
        return loadScene (filename);
    }
    else if (strcmp(type, "video") == 0)
    {
        return loadVideo (filename);
    }

    throw std::runtime_error("Unsupported wallpaper type");
}

CScene* CScene::loadScene (const irr::io::path& filename)
{
    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename));

    auto camera_it = jsonFindRequired (content, "camera", "Scenes must have a defined camera");
    auto general_it = jsonFindRequired (content, "general", "Scenes must have a general section");
    auto objects_it = jsonFindRequired (content, "objects", "Scenes must have a list of objects to display");

    auto ambientcolor_it = jsonFindRequired (*general_it, "ambientcolor", "General section must have ambient color");
    auto bloom_it = jsonFindRequired (*general_it, "bloom", "General section must have bloom flag");
    auto bloomstrength_it = jsonFindRequired (*general_it, "bloomstrength", "General section must have bloom strength");
    auto bloomthreshold_it = jsonFindRequired (*general_it, "bloomthreshold", "General section must have bloom threshold");
    auto camerafade_it = jsonFindRequired (*general_it, "camerafade", "General section must have camera fade");
    auto cameraparallax_it = jsonFindRequired (*general_it, "cameraparallax", "General section must have camera parallax");
    auto cameraparallaxamount_it = jsonFindRequired (*general_it, "cameraparallaxamount", "General section must have camera parallax amount");
    auto cameraparallaxdelay_it = jsonFindRequired (*general_it, "cameraparallaxdelay", "General section must have camera parallax delay");
    auto cameraparallaxmouseinfluence_it = jsonFindRequired (*general_it, "cameraparallaxmouseinfluence", "General section must have camera parallax mouse influence");
    auto camerapreview_it = jsonFindRequired (*general_it, "camerapreview", "General section must have camera preview");
    auto camerashake_it = jsonFindRequired (*general_it, "camerashake", "General section must have camera shake");
    auto camerashakeamplitude_it = jsonFindRequired (*general_it, "camerashakeamplitude", "General section must have camera shake amplitude");
    auto camerashakeroughness_it = jsonFindRequired (*general_it, "camerashakeroughness", "General section must have camera shake roughness");
    auto camerashakespeed_it = jsonFindRequired (*general_it, "camerashakespeed", "General section must have camera shake speed");
    auto clearcolor_it = jsonFindRequired (*general_it, "clearcolor", "General section must have clear color");
    auto orthogonalprojection_it = jsonFindRequired (*general_it, "orthogonalprojection", "General section must have orthogonal projection info");
    auto skylightcolor_it = jsonFindRequired (*general_it, "skylightcolor", "General section must have skylight color");

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

    auto cur = (*objects_it).begin ();
    auto end = (*objects_it).end ();

    for (; cur != end; cur ++)
    {
        scene->insertObject (
                CObject::fromJSON (*cur)
        );
    }

    return scene;
}

CScene* CScene::loadVideo (const irr::io::path& filename)
{
    return nullptr;
}

const std::vector<CObject*>& CScene::getObjects () const
{
    return this->m_objects;
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

const Scenes::CCamera* CScene::getCamera () const
{
    return this->m_camera;
}

const irr::video::SColorf &CScene::getAmbientColor() const
{
    return this->m_ambientColor;
}

const bool CScene::isBloom () const
{
    return this->m_bloom;
}

const irr::f64 CScene::getBloomStrength () const
{
    return this->m_bloomStrength;
}

const irr::f64 CScene::getBloomThreshold () const
{
    return this->m_bloomThreshold;
}

const bool CScene::isCameraFade () const
{
    return this->m_cameraFade;
}

const bool CScene::isCameraParallax () const
{
    return this->m_cameraParallax;
}

const irr::f64 CScene::getCameraParallaxAmount () const
{
    return this->m_cameraParallaxAmount;
}

const irr::f64 CScene::getCameraParallaxDelay () const
{
    return this->m_cameraParallaxDelay;
}

const irr::f64 CScene::getCameraParallaxMouseInfluence () const
{
    return this->m_cameraParallaxMouseInfluence;
}

const bool CScene::isCameraPreview () const
{
    return this->m_cameraPreview;
}

const bool CScene::isCameraShake () const
{
    return this->m_cameraShake;
}

const irr::f64 CScene::getCameraShakeAmplitude () const
{
    return this->m_cameraShakeAmplitude;
}

const irr::f64 CScene::getCameraShakeRoughness () const
{
    return this->m_cameraShakeRoughness;
}

const irr::f64 CScene::getCameraShakeSpeed () const
{
    return this->m_cameraShakeSpeed;
}

const irr::video::SColorf& CScene::getClearColor () const
{
    return this->m_clearColor;
}

const Scenes::CProjection* CScene::getOrthogonalProjection () const
{
    return this->m_orthogonalProjection;
}

const irr::video::SColorf& CScene::getSkylightColor () const
{
    return this->m_skylightColor;
}
