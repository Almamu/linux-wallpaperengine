#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "project.h"
#include "object.h"

#include "scenes/camera.h"
#include "scenes/projection.h"

namespace wp::core
{
    using json = nlohmann::json;

    class project;
    class object;

    class scene
    {
    public:
        static scene* fromFile (const irr::io::path& filename);

        project* getProject ();
        std::vector<object*>* getObjects ();


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
        scenes::projection *getOrthogonalProjection() const;
        const irr::video::SColorf &getSkylightColor() const;

    protected:
        friend class project;

        void setProject (project* project);
        scenes::camera* getCamera ();

        scene (
            scenes::camera* camera,
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
            scenes::projection* orthogonalProjection,
            irr::video::SColorf skylightColor
        );

        void insertObject (object* object);
    private:
        project* m_project;
        scenes::camera* m_camera;

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
        scenes::projection* m_orthogonalProjection;
        irr::video::SColorf m_skylightColor;

        std::vector<object*> m_objects;
    };
};
