#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <stdexcept>

#include "wallpaperengine/fs/utils.h"

#include "project.h"
#include "irrlicht.h"

namespace wp
{
    project::project (irr::io::path& jsonfile_path)
    {
        this->m_content = wp::fs::utils::loadFullFile (jsonfile_path);
        this->m_projectFile = json::parse (this->m_content);

        json::const_iterator file_it = this->m_projectFile.find ("file");
        json::const_iterator name_it = this->m_projectFile.find ("title");
        json::const_iterator type_it = this->m_projectFile.find ("type");

        if (type_it != this->m_projectFile.end ())
        {
            this->m_type = type_it.value ();
        }

        if (name_it != this->m_projectFile.end ())
        {
            this->m_title = name_it.value ();
        }

        std::transform(this->m_type.begin(), this->m_type.end(), this->m_type.begin(), tolower);
        if (this->m_type != "scene")
        {
            throw std::runtime_error ("Only scene wallpapers are supported");
        }

        if (file_it != this->m_projectFile.end ())
        {
            // load scene file
            this->m_file = (*file_it).get <std::string> ().c_str ();
            this->m_scene = new scene (this->m_file);
        }
    }

    scene* project::getScene ()
    {
        return this->m_scene;
    }
}