#include <wallpaperengine/fs/utils.h>

#include "project.h"

#include "../fs/utils.h"

using namespace wp::core;

project::project (std::string title, std::string type, scene *scene) :
    m_title (std::move (title)),
    m_type (std::move (type)),
    m_scene (scene)
{
    this->m_scene->setProject (this);
}

project* project::fromFile (const irr::io::path& filename)
{
    json content = json::parse (wp::fs::utils::loadFullFile (filename));

    json::const_iterator title = content.find ("title");
    json::const_iterator type = content.find ("type");
    json::const_iterator file = content.find ("file");

    if (title == content.end ())
    {
        throw std::runtime_error ("Project title missing");
    }

    if (type == content.end ())
    {
        throw std::runtime_error ("Project type missing");
    }

    if (file == content.end ())
    {
        throw std::runtime_error ("Project's main file missing");
    }

    return new project (
        *title,
        *type,
        scene::fromFile ((*file).get <std::string> ().c_str ())
    );
}

scene* project::getScene ()
{
    return this->m_scene;
}

std::string project::getTitle ()
{
    return this->m_title;
}

std::string project::getType ()
{
    return this->m_type;
}