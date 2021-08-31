#pragma once

#include "CObject.h"
#include "CWallpaper.h"

#include "Core.h"

#include "WallpaperEngine/Core/Scenes/CCamera.h"
#include "WallpaperEngine/Core/Scenes/CProjection.h"
#include "WallpaperEngine/Core/Types/FloatColor.h"

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Core::Types;

    class CObject;

    class CScene : public CWallpaper
    {
    public:
        static CScene* fromFile (const std::string& filename, CContainer* container);

        const std::vector<CObject*>& getObjects () const;

        const FloatColor& getAmbientColor() const;
        const bool isBloom() const;
        const double getBloomStrength() const;
        const double getBloomThreshold() const;
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
        const FloatColor& getClearColor() const;
        const Scenes::CProjection* getOrthogonalProjection() const;
        const FloatColor& getSkylightColor() const;
        const Scenes::CCamera* getCamera () const;

    protected:
        friend class CWallpaper;

        CScene (
                CContainer* container,
                Scenes::CCamera* camera,
                FloatColor ambientColor,
                bool bloom,
                double bloomStrength,
                double bloomThreshold,
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
                FloatColor clearColor,
                Scenes::CProjection* orthogonalProjection,
                FloatColor skylightColor
        );

        static const std::string Type;

        void insertObject (CObject* object);

        CContainer* getContainer ();
    private:
        CContainer* m_container;
        Scenes::CCamera* m_camera;

        // data from general section on the json
        FloatColor m_ambientColor;
        bool m_bloom;
        double m_bloomStrength;
        double m_bloomThreshold;
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
        FloatColor m_clearColor;
        Scenes::CProjection* m_orthogonalProjection;
        FloatColor m_skylightColor;

        std::vector<CObject*> m_objects;
    };
};
