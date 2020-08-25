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
    CWallpaper (Type),
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
    std::string stringContent = WallpaperEngine::FileSystem::loadFullFile (filename);
    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename));

    auto camera_it = jsonFindRequired (content, "camera", "Scenes must have a defined camera");
    auto general_it = jsonFindRequired (content, "general", "Scenes must have a general section");
    auto objects_it = jsonFindRequired (content, "objects", "Scenes must have a list of objects to display");

    // TODO: FIND IF THESE DEFAULTS ARE SENSIBLE OR NOT AND PERFORM PROPER VALIDATION WHEN CAMERA PREVIEW AND CAMERA PARALLAX ARE PRESENT
    auto ambientcolor_it = jsonFindRequired (*general_it, "ambientcolor", "General section must have ambient color");
    auto bloom_it = jsonFindRequired (*general_it, "bloom", "General section must have bloom flag");
    auto bloomstrength_it = jsonFindRequired (*general_it, "bloomstrength", "General section must have bloom strength");
    auto bloomthreshold_it = jsonFindRequired (*general_it, "bloomthreshold", "General section must have bloom threshold");
    auto camerafade_it = jsonFindRequired (*general_it, "camerafade", "General section must have camera fade");
    auto cameraparallax = jsonFindDefault <bool> (*general_it, "cameraparallax", true);
    auto cameraparallaxamount = jsonFindDefault <irr::f64> (*general_it, "cameraparallaxamount", 1.0f);
    auto cameraparallaxdelay = jsonFindDefault <irr::f64> (*general_it, "cameraparallaxdelay", 0.0f);
    auto cameraparallaxmouseinfluence = jsonFindDefault <irr::f64> (*general_it, "cameraparallaxmouseinfluence", 1.0f);
    auto camerapreview = jsonFindDefault <bool> (*general_it, "camerapreview", false);
    auto camerashake = jsonFindDefault <bool> (*general_it, "camerashake", false);
    auto camerashakeamplitude = jsonFindDefault <irr::f64> (*general_it, "camerashakeamplitude", 0.0f);
    auto camerashakeroughness = jsonFindDefault <irr::f64> (*general_it, "camerashakeroughness", 0.0f);
    auto camerashakespeed = jsonFindDefault <irr::f64> (*general_it, "camerashakespeed", 0.0f);
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
            cameraparallax,
            cameraparallaxamount,
            cameraparallaxdelay,
            cameraparallaxmouseinfluence,
            camerapreview,
            camerashake,
            camerashakeamplitude,
            camerashakeroughness,
            camerashakespeed,
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

const std::vector<CObject*>& CScene::getObjects () const
{
    return this->m_objects;
}

void CScene::insertObject (CObject* object)
{
    /// TODO: XXXHACK -- TO REMOVE WHEN PARTICLE SUPPORT IS PROPERLY IMPLEMENTED
    if (object != nullptr)
        this->m_objects.push_back (object);
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

const std::string CScene::Type = "scene";
