#include "CScene.h"
#include "WallpaperEngine/Core/CProject.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Wallpapers;

CScene::CScene (
    const CProject& project, const CContainer* container, const Scenes::CCamera* camera, glm::vec3 ambientColor,
    const CUserSettingBoolean* bloom, const CUserSettingFloat* bloomStrength, const CUserSettingFloat* bloomThreshold,
    bool cameraFade, bool cameraParallax, float cameraParallaxAmount, float cameraParallaxDelay,
    float cameraParallaxMouseInfluence, bool cameraPreview, bool cameraShake, float cameraShakeAmplitude,
    float cameraShakeRoughness, float cameraShakeSpeed, const CUserSettingVector3* clearColor,
    const Scenes::CProjection* orthogonalProjection, glm::vec3 skylightColor
) :
    CWallpaper (Type, project),
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
    m_skylightColor (skylightColor) {}

const CScene* CScene::fromFile (const std::string& filename, const CProject& project, const CContainer* container) {
    json content = json::parse (container->readFileAsString (filename));

    const auto general_it = jsonFindRequired (content, "general", "Scenes must have a general section");
    const auto objects_it = jsonFindRequired (content, "objects", "Scenes must have a list of objects to display");

    // TODO: FIND IF THESE DEFAULTS ARE SENSIBLE OR NOT AND PERFORM PROPER VALIDATION WHEN CAMERA PREVIEW AND CAMERA
    // PARALLAX ARE PRESENT

    auto* scene = new CScene (
        project, container,
        Scenes::CCamera::fromJSON (jsonFindRequired (content, "camera", "Scenes must have a defined camera")),
        jsonFindDefault<glm::vec3> (*general_it, "ambientcolor", glm::vec3 (0, 0, 0)),
        jsonFindUserConfig<CUserSettingBoolean> (*general_it, project, "bloom", false),
        jsonFindUserConfig<CUserSettingFloat> (*general_it, project, "bloomstrength", 0.0),
        jsonFindUserConfig<CUserSettingFloat> (*general_it, project, "bloomthreshold", 0.0),
        jsonFindDefault<bool> (*general_it, "camerafade", false),
        jsonFindDefault<bool> (*general_it, "cameraparallax", true),
        jsonFindDefault<float> (*general_it, "cameraparallaxamount", 1.0f),
        jsonFindDefault<float> (*general_it, "cameraparallaxdelay", 0.0f),
        jsonFindDefault<float> (*general_it, "cameraparallaxmouseinfluence", 1.0f),
        jsonFindDefault<bool> (*general_it, "camerapreview", false),
        jsonFindDefault<bool> (*general_it, "camerashake", false),
        jsonFindDefault<float> (*general_it, "camerashakeamplitude", 0.0f),
        jsonFindDefault<float> (*general_it, "camerashakeroughness", 0.0f),
        jsonFindDefault<float> (*general_it, "camerashakespeed", 0.0f),
        jsonFindUserConfig<CUserSettingVector3> (*general_it, project, "clearcolor", {1, 1, 1}),
        Scenes::CProjection::fromJSON (jsonFindRequired (*general_it, "orthogonalprojection", "General section must have orthogonal projection info")),
        jsonFindDefault<glm::vec3> (*general_it, "skylightcolor", glm::vec3 (0, 0, 0))
    );

    for (const auto& cur : *objects_it)
        scene->insertObject (CObject::fromJSON (cur, scene, container));

    return scene;
}

const std::map<uint32_t, const CObject*>& CScene::getObjects () const {
    return this->m_objects;
}

const std::vector<const CObject*>& CScene::getObjectsByRenderOrder () const {
    return this->m_objectsByRenderOrder;
}

void CScene::insertObject (const CObject* object) {
    /// TODO: XXXHACK -- TO REMOVE WHEN PARTICLE SUPPORT IS PROPERLY IMPLEMENTED
    if (object != nullptr) {
        this->m_objects.insert (std::pair (object->getId (), object));
        this->m_objectsByRenderOrder.emplace_back (object);
    }
}

const CContainer* CScene::getContainer () const {
    return this->m_container;
}

const Scenes::CCamera* CScene::getCamera () const {
    return this->m_camera;
}

const glm::vec3& CScene::getAmbientColor () const {
    return this->m_ambientColor;
}

bool CScene::isBloom () const {
    return this->m_bloom->getBool ();
}

float CScene::getBloomStrength () const {
    return this->m_bloomStrength->getFloat ();
}

float CScene::getBloomThreshold () const {
    return this->m_bloomThreshold->getFloat ();
}

bool CScene::isCameraFade () const {
    return this->m_cameraFade;
}

bool CScene::isCameraParallax () const {
    return this->m_cameraParallax;
}

float CScene::getCameraParallaxAmount () const {
    return this->m_cameraParallaxAmount;
}

float CScene::getCameraParallaxDelay () const {
    return this->m_cameraParallaxDelay;
}

float CScene::getCameraParallaxMouseInfluence () const {
    return this->m_cameraParallaxMouseInfluence;
}

bool CScene::isCameraPreview () const {
    return this->m_cameraPreview;
}

bool CScene::isCameraShake () const {
    return this->m_cameraShake;
}

float CScene::getCameraShakeAmplitude () const {
    return this->m_cameraShakeAmplitude;
}

float CScene::getCameraShakeRoughness () const {
    return this->m_cameraShakeRoughness;
}

float CScene::getCameraShakeSpeed () const {
    return this->m_cameraShakeSpeed;
}

const glm::vec3& CScene::getClearColor () const {
    return this->m_clearColor->getVec3 ();
}

const Scenes::CProjection* CScene::getOrthogonalProjection () const {
    return this->m_orthogonalProjection;
}

const glm::vec3& CScene::getSkylightColor () const {
    return this->m_skylightColor;
}

const std::string CScene::Type = "scene";
