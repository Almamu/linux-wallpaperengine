#include "CScene.h"
#include "CProject.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"
#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Core;

CScene::CScene (
        CContainer* container,
        Scenes::CCamera* camera,
        glm::vec3 ambientColor,
        CUserSettingBoolean* bloom,
        CUserSettingFloat* bloomStrength,
        CUserSettingFloat* bloomThreshold,
        bool cameraFade,
        bool cameraParallax,
        double cameraParallaxAmount,
        double cameraParallaxDelay,
        double cameraParallaxMouseInfluence,
        bool cameraPreview,
        bool cameraShake,
        double cameraShakeAmplitude,
        double cameraShakeRoughness,
        double cameraShakeSpeed,
                CUserSettingVector3* clearColor,
        Scenes::CProjection* orthogonalProjection,
        glm::vec3 skylightColor) :
    CWallpaper (Type),
    m_container (container),
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

CScene* CScene::fromFile (const std::string& filename, CContainer* container)
{
    std::string stringContent = WallpaperEngine::FileSystem::loadFullFile (filename, container);
    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container));

    auto camera_it = jsonFindRequired (content, "camera", "Scenes must have a defined camera");
    auto general_it = jsonFindRequired (content, "general", "Scenes must have a general section");
    auto objects_it = jsonFindRequired (content, "objects", "Scenes must have a list of objects to display");

    // TODO: FIND IF THESE DEFAULTS ARE SENSIBLE OR NOT AND PERFORM PROPER VALIDATION WHEN CAMERA PREVIEW AND CAMERA PARALLAX ARE PRESENT
    auto ambientcolor = jsonFindDefault <std::string> (*general_it, "ambientcolor", "0 0 0");
    auto bloom = jsonFindUserConfig <CUserSettingBoolean, bool> (*general_it, "bloom", false);
    auto bloomstrength = jsonFindUserConfig <CUserSettingFloat, double> (*general_it, "bloomstrength", 0.0);
    auto bloomthreshold = jsonFindUserConfig <CUserSettingFloat, double> (*general_it, "bloomthreshold", 0.0);
    auto camerafade = jsonFindDefault <bool> (*general_it, "camerafade", false);
    auto cameraparallax = jsonFindDefault <bool> (*general_it, "cameraparallax", true);
    auto cameraparallaxamount = jsonFindDefault <double> (*general_it, "cameraparallaxamount", 1.0f);
    auto cameraparallaxdelay = jsonFindDefault <double> (*general_it, "cameraparallaxdelay", 0.0f);
    auto cameraparallaxmouseinfluence = jsonFindDefault <double> (*general_it, "cameraparallaxmouseinfluence", 1.0f);
    auto camerapreview = jsonFindDefault <bool> (*general_it, "camerapreview", false);
    auto camerashake = jsonFindDefault <bool> (*general_it, "camerashake", false);
    auto camerashakeamplitude = jsonFindDefault <double> (*general_it, "camerashakeamplitude", 0.0f);
    auto camerashakeroughness = jsonFindDefault <double> (*general_it, "camerashakeroughness", 0.0f);
    auto camerashakespeed = jsonFindDefault <double> (*general_it, "camerashakespeed", 0.0f);
    auto clearcolor = jsonFindUserConfig <CUserSettingVector3, glm::vec3> (*general_it, "clearcolor", {1, 1, 1});
    auto orthogonalprojection_it = jsonFindRequired (*general_it, "orthogonalprojection", "General section must have orthogonal projection info");
    auto skylightcolor = jsonFindDefault <std::string> (*general_it, "skylightcolor", "0 0 0");

    CScene* scene = new CScene (
            container,
            Scenes::CCamera::fromJSON (*camera_it),
            WallpaperEngine::Core::aToColorf(ambientcolor),
            bloom,
            bloomstrength,
            bloomthreshold,
            camerafade,
            cameraparallax,
            cameraparallaxamount,
            cameraparallaxdelay,
            cameraparallaxmouseinfluence,
            camerapreview,
            camerashake,
            camerashakeamplitude,
            camerashakeroughness,
            camerashakespeed,
            clearcolor,
            Scenes::CProjection::fromJSON (*orthogonalprojection_it),
            WallpaperEngine::Core::aToColorf(skylightcolor)
    );

    for (const auto& cur : *objects_it)
        scene->insertObject (CObject::fromJSON (cur, scene, container));

    return scene;
}

const std::map<uint32_t, CObject*>& CScene::getObjects () const
{
    return this->m_objects;
}
const std::vector<CObject*>& CScene::getObjectsByRenderOrder () const
{
    return this->m_objectsByRenderOrder;
}

void CScene::insertObject (CObject* object)
{
    /// TODO: XXXHACK -- TO REMOVE WHEN PARTICLE SUPPORT IS PROPERLY IMPLEMENTED
    if (object != nullptr)
    {
        this->m_objects.insert (std::make_pair (object->getId (), object));
        this->m_objectsByRenderOrder.emplace_back (object);
    }
}

CContainer* CScene::getContainer ()
{
    return this->m_container;
}

const Scenes::CCamera* CScene::getCamera () const
{
    return this->m_camera;
}

const glm::vec3 &CScene::getAmbientColor() const
{
    return this->m_ambientColor;
}

const bool CScene::isBloom () const
{
    return this->m_bloom->processValue (this->getProject ()->getProperties ());
}

double CScene::getBloomStrength () const
{
    return this->m_bloomStrength->processValue (this->getProject ()->getProperties ());
}

double CScene::getBloomThreshold () const
{
    return this->m_bloomThreshold->processValue (this->getProject ()->getProperties ());
}

const bool CScene::isCameraFade () const
{
    return this->m_cameraFade;
}

const bool CScene::isCameraParallax () const
{
    return this->m_cameraParallax;
}

const double CScene::getCameraParallaxAmount () const
{
    return this->m_cameraParallaxAmount;
}

const double CScene::getCameraParallaxDelay () const
{
    return this->m_cameraParallaxDelay;
}

const double CScene::getCameraParallaxMouseInfluence () const
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

const double CScene::getCameraShakeAmplitude () const
{
    return this->m_cameraShakeAmplitude;
}

const double CScene::getCameraShakeRoughness () const
{
    return this->m_cameraShakeRoughness;
}

const double CScene::getCameraShakeSpeed () const
{
    return this->m_cameraShakeSpeed;
}

glm::vec3 CScene::getClearColor () const
{
    return this->m_clearColor->processValue (this->getProject ()->getProperties ());
}

Scenes::CProjection* CScene::getOrthogonalProjection () const
{
    return this->m_orthogonalProjection;
}

const glm::vec3& CScene::getSkylightColor () const
{
    return this->m_skylightColor;
}

const std::string CScene::Type = "scene";
