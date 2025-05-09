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
    CScene (
        std::shared_ptr <const Core::CProject> project, std::shared_ptr<const CContainer> container,
        const Scenes::CCamera* camera, glm::vec3 ambientColor, const CUserSettingBoolean* bloom,
        const CUserSettingFloat* bloomStrength, const CUserSettingFloat* bloomThreshold, bool cameraFade,
        bool cameraParallax, float cameraParallaxAmount, float cameraParallaxDelay, float cameraParallaxMouseInfluence,
        bool cameraPreview, bool cameraShake, float cameraShakeAmplitude, float cameraShakeRoughness,
        float cameraShakeSpeed, const CUserSettingVector3* clearColor, const Scenes::CProjection* orthogonalProjection,
        glm::vec3 skylightColor);

    static std::shared_ptr <const CScene> fromFile (
        const std::string& filename, std::shared_ptr <const Core::CProject> project,
        const std::shared_ptr<const CContainer>& container);
    [[nodiscard]] const std::map<uint32_t, const CObject*>& getObjects () const;

    [[nodiscard]] const std::vector<const CObject*>& getObjectsByRenderOrder () const;
    [[nodiscard]] const glm::vec3& getAmbientColor () const;
    [[nodiscard]] bool isBloom () const;
    [[nodiscard]] float getBloomStrength () const;
    [[nodiscard]] float getBloomThreshold () const;
    [[nodiscard]] bool isCameraFade () const;
    [[nodiscard]] bool isCameraParallax () const;
    [[nodiscard]] float getCameraParallaxAmount () const;
    [[nodiscard]] float getCameraParallaxDelay () const;
    [[nodiscard]] float getCameraParallaxMouseInfluence () const;
    [[nodiscard]] bool isCameraPreview () const;
    [[nodiscard]] bool isCameraShake () const;
    [[nodiscard]] float getCameraShakeAmplitude () const;
    [[nodiscard]] float getCameraShakeRoughness () const;
    [[nodiscard]] float getCameraShakeSpeed () const;
    [[nodiscard]] const glm::vec3& getClearColor () const;
    [[nodiscard]] const Scenes::CProjection* getOrthogonalProjection () const;
    [[nodiscard]] const glm::vec3& getSkylightColor () const;

    [[nodiscard]] const Scenes::CCamera* getCamera () const;

  protected:
    friend class CWallpaper;

    void insertObject (const CObject* object);

    [[nodiscard]] std::shared_ptr<const CContainer> getContainer () const;

  private:
    const std::shared_ptr<const CContainer> m_container;
    const Scenes::CCamera* m_camera;

    // data from general section on the json
    const glm::vec3 m_ambientColor;
    const CUserSettingBoolean* m_bloom;
    const CUserSettingFloat* m_bloomStrength;
    const CUserSettingFloat* m_bloomThreshold;
    const bool m_cameraFade;
    const bool m_cameraParallax;
    const float m_cameraParallaxAmount;
    const float m_cameraParallaxDelay;
    const float m_cameraParallaxMouseInfluence;
    const bool m_cameraPreview;
    const bool m_cameraShake;
    const float m_cameraShakeAmplitude;
    const float m_cameraShakeRoughness;
    const float m_cameraShakeSpeed;
    const CUserSettingVector3* m_clearColor;
    const Scenes::CProjection* m_orthogonalProjection;
    const glm::vec3 m_skylightColor;

    std::map<uint32_t, const CObject*> m_objects = {};
    std::vector<const CObject*> m_objectsByRenderOrder = {};
};
} // namespace WallpaperEngine::Core
