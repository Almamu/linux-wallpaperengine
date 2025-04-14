#pragma once

#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/CWallpaper.h"

#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Scenes/CCamera.h"
#include "WallpaperEngine/Core/Scenes/CProjection.h"

namespace WallpaperEngine::Core {
class CObject;
}

namespace WallpaperEngine::Core::Wallpapers {
using json = nlohmann::json;

class CScene : public CWallpaper {
  public:
    static const CScene* fromFile (std::string filename, const CProject& project, const CContainer* container);

    [[nodiscard]] const std::map<uint32_t, const CObject*>& getObjects () const;
    [[nodiscard]] const std::vector<const CObject*>& getObjectsByRenderOrder () const;

    [[nodiscard]] const glm::vec3& getAmbientColor () const;
    [[nodiscard]] bool isBloom () const;
    [[nodiscard]] double getBloomStrength () const;
    [[nodiscard]] double getBloomThreshold () const;
    [[nodiscard]] bool isCameraFade () const;
    [[nodiscard]] bool isCameraParallax () const;
    [[nodiscard]] double getCameraParallaxAmount () const;
    [[nodiscard]] double getCameraParallaxDelay () const;
    [[nodiscard]] double getCameraParallaxMouseInfluence () const;
    [[nodiscard]] bool isCameraPreview () const;
    [[nodiscard]] bool isCameraShake () const;
    [[nodiscard]] double getCameraShakeAmplitude () const;
    [[nodiscard]] double getCameraShakeRoughness () const;
    [[nodiscard]] double getCameraShakeSpeed () const;
    [[nodiscard]] const glm::vec3& getClearColor () const;
    [[nodiscard]] const Scenes::CProjection* getOrthogonalProjection () const;
    [[nodiscard]] const glm::vec3& getSkylightColor () const;
    [[nodiscard]] const Scenes::CCamera* getCamera () const;

  protected:
    friend class CWallpaper;

    CScene (
        const CProject& project, const CContainer* container, const Scenes::CCamera* camera, glm::vec3 ambientColor,
        const CUserSettingBoolean* bloom, const CUserSettingFloat* bloomStrength, const CUserSettingFloat* bloomThreshold,
        bool cameraFade, bool cameraParallax, double cameraParallaxAmount, double cameraParallaxDelay,
        double cameraParallaxMouseInfluence, bool cameraPreview, bool cameraShake, double cameraShakeAmplitude,
        double cameraShakeRoughness, double cameraShakeSpeed, const CUserSettingVector3* clearColor,
        const Scenes::CProjection* orthogonalProjection, glm::vec3 skylightColor);

    static const std::string Type;

    void insertObject (const CObject* object);

    const CContainer* getContainer () const;

  private:
    const CContainer* m_container;
    const Scenes::CCamera* m_camera;

    // data from general section on the json
    const glm::vec3 m_ambientColor;
    const CUserSettingBoolean* m_bloom;
    const CUserSettingFloat* m_bloomStrength;
    const CUserSettingFloat* m_bloomThreshold;
    const bool m_cameraFade;
    const bool m_cameraParallax;
    const double m_cameraParallaxAmount;
    const double m_cameraParallaxDelay;
    const double m_cameraParallaxMouseInfluence;
    const bool m_cameraPreview;
    const bool m_cameraShake;
    const double m_cameraShakeAmplitude;
    const double m_cameraShakeRoughness;
    const double m_cameraShakeSpeed;
    const CUserSettingVector3* m_clearColor;
    const Scenes::CProjection* m_orthogonalProjection;
    const glm::vec3 m_skylightColor;

    std::map<uint32_t, const CObject*> m_objects;
    std::vector<const CObject*> m_objectsByRenderOrder;
};
} // namespace WallpaperEngine::Core
