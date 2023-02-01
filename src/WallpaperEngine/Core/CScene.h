#pragma once

#include "CObject.h"
#include "CWallpaper.h"

#include "Core.h"

#include "WallpaperEngine/Core/Scenes/CCamera.h"
#include "WallpaperEngine/Core/Scenes/CProjection.h"

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;

    class CObject;

    class CScene : public CWallpaper
    {
    public:
        static CScene* fromFile (const std::string& filename, CContainer* container);

        const std::map<uint32_t, CObject*>& getObjects () const;
        const std::vector<CObject*>& getObjectsByRenderOrder () const;

        const glm::vec3& getAmbientColor() const;
        const bool isBloom() const;
        double getBloomStrength() const;
        double getBloomThreshold() const;
        const bool isCameraFade() const;
        const bool isCameraParallax() const;
        const double getCameraParallaxAmount() const;
        const double getCameraParallaxDelay() const;
        const double getCameraParallaxMouseInfluence() const;
        const bool isCameraPreview() const;
        const bool isCameraShake() const;
        const double getCameraShakeAmplitude() const;
        const double getCameraShakeRoughness() const;
        const double getCameraShakeSpeed() const;
        glm::vec3 getClearColor() const;
        Scenes::CProjection* getOrthogonalProjection() const;
        const glm::vec3& getSkylightColor() const;
        const Scenes::CCamera* getCamera () const;

    protected:
        friend class CWallpaper;

        CScene (
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
                glm::vec3 skylightColor
        );

        static const std::string Type;

        void insertObject (CObject* object);

        CContainer* getContainer ();
    private:
        CContainer* m_container;
        Scenes::CCamera* m_camera;

        // data from general section on the json
        glm::vec3 m_ambientColor;
        CUserSettingBoolean* m_bloom;
        CUserSettingFloat* m_bloomStrength;
        CUserSettingFloat* m_bloomThreshold;
        bool m_cameraFade;
        bool m_cameraParallax;
        double m_cameraParallaxAmount;
        double m_cameraParallaxDelay;
        double m_cameraParallaxMouseInfluence;
        bool m_cameraPreview;
        bool m_cameraShake;
        double m_cameraShakeAmplitude;
        double m_cameraShakeRoughness;
        double m_cameraShakeSpeed;
        CUserSettingVector3* m_clearColor;
        Scenes::CProjection* m_orthogonalProjection;
        glm::vec3 m_skylightColor;

        std::map<uint32_t, CObject*> m_objects;
        std::vector<CObject*> m_objectsByRenderOrder;
    };
};
