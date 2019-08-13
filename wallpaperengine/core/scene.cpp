#include "scene.h"
#include "project.h"

#include "../fs/utils.h"

using namespace wp::core;

scene::scene (scenes::camera* camera) :
    m_camera (camera)
{

}

scene* scene::fromFile (irr::io::path filename)
{
    json content = json::parse (wp::fs::utils::loadFullFile (filename));

    json::const_iterator camera_it = content.find ("camera");

    if (camera_it == content.end ())
    {
        throw std::runtime_error ("Scenes must have a defined camera");
    }

    return new scene (
        scenes::camera::fromJSON (*camera_it)
    );
}

project* scene::getProject ()
{
    return this->m_project;
}

void scene::setProject (project* project)
{
    this->m_project = project;
}

scenes::camera* scene::getCamera ()
{
    return this->m_camera;
}