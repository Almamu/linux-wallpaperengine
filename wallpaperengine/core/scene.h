#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "project.h"
#include "scenes/camera.h"

namespace wp::core
{
    using json = nlohmann::json;

    class project;

    class scene
    {
    public:
        static scene* fromFile (irr::io::path filename);

        project* getProject ();
    protected:
        friend class project;

        void setProject (project* project);
        scenes::camera* getCamera ();

        scene (scenes::camera* camera);
    private:
        project* m_project;
        scenes::camera* m_camera;
    };
};
