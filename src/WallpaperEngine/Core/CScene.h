#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "CProject.h"
#include "CObject.h"

#include "WallpaperEngine/Core/Scenes/CCamera.h"
#include "WallpaperEngine/Core/Scenes/CProjection.h"

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;

    class CProject;
    class CObject;

    class CScene
    {
    public:
        static CScene* fromFile (const irr::io::path& filename);

        CProject* getProject ();
        std::vector<CObject*>* getObjects ();

        const irr::video::SColorf &getAmbientColor() const;
        bool isBloom() const;
        irr::f64 getBloomStrength() const;
        irr::f64 getBloomThreshold() const;
        bool isCameraFade() const;
        bool isCameraParallax() const;
        irr::f64 getCameraParallaxAmount() const;
        irr::f64 getCameraParallaxDelay() const;
        irr::f64 getCameraParallaxMouseInfluence() const;
        bool isCameraPreview() const;
        bool isCameraShake() const;
        irr::f64 getCameraShakeAmplitude() const;
        irr::f64 getCameraShakeRoughness() const;
        irr::f64 getCameraShakeSpeed() const;
        const irr::video::SColorf &getClearColor() const;
        Scenes::CProjection *getOrthogonalProjection() const;
        const irr::video::SColorf &getSkylightColor() const;
        Scenes::CCamera* getCamera ();

    protected:
        friend class CProject;

        void setProject (CProject* project);

        CScene (
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
                irr::video::SColorf skylightColor
        );

        void insertObject (CObject* object);
    private:
        CProject* m_project;
        Scenes::CCamera* m_camera;

        // data from general section on the json
        irr::video::SColorf m_ambientColor;
        bool m_bloom;
        irr::f64 m_bloomStrength;
        irr::f64 m_bloomThreshold;
        bool m_cameraFade;
        bool m_cameraParallax;
        irr::f64 m_cameraParallaxAmount;
        irr::f64 m_cameraParallaxDelay;
        irr::f64 m_cameraParallaxMouseInfluence;
        bool m_cameraPreview;
        bool m_cameraShake;
        irr::f64 m_cameraShakeAmplitude;
        irr::f64 m_cameraShakeRoughness;
        irr::f64 m_cameraShakeSpeed;
        irr::video::SColorf m_clearColor;
        Scenes::CProjection* m_orthogonalProjection;
        irr::video::SColorf m_skylightColor;

        std::vector<CObject*> m_objects;
    };
};
