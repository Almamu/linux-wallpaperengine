#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

#include <wallpaperengine/scene.h>
#include <wallpaperengine/camera.h>

#include "wallpaperengine/fs/utils.h"

namespace wp
{
    using json = nlohmann::json;

    scene::scene (irr::io::path& file)
    {
        this->m_content = wp::fs::utils::loadFullFile (file);
        this->m_json = json::parse (this->m_content);
#if 0
        // dump scene json to file
        std::string path = std::string (getenv("HOME")) + "/stuff/wallpaperengine-dumps/";
        system (("mkdir -p " + path).c_str ());
        path += file.c_str ();
        std::ofstream out = std::ofstream (path);
        out << this->m_json.dump (4) << std::endl;
#endif

        // check basic elements
        json::const_iterator camera_it =    this->m_json.find ("camera");
        json::const_iterator general_it =   this->m_json.find ("general");
        json::const_iterator objects_it =   this->m_json.find ("objects");

        if (camera_it != this->m_json.end () && objects_it.value ().is_array () == true)
        {
            this->m_camera = new camera (*camera_it);
        }

        // read orthogonalprojection before loading objects so they can set their vertices properly
        json::const_iterator orthogonalprojection = (*general_it).find ("orthogonalprojection");

        if (orthogonalprojection != (*general_it).end () && (*orthogonalprojection).is_object () == true)
        {
            json::const_iterator width = (*orthogonalprojection).find ("width");
            json::const_iterator height = (*orthogonalprojection).find ("height");

            if (width != (*orthogonalprojection).end () && (*width).is_number () == true)
            {
                this->m_width = *width;
            }

            if (height != (*orthogonalprojection).end () && (*height).is_number () == true)
            {
                this->m_height= *height;
            }

            this->m_isOrthogonal = true;
        }

        if (objects_it != this->m_json.end () && objects_it.value ().is_array () == true)
        {
            json::const_iterator cur = this->m_json ["objects"].begin ();
            json::const_iterator end = this->m_json ["objects"].end ();

            for (; cur != end; cur ++)
            {
                this->m_objects.push_back (new object (*cur, this));
            }
        }
    }

    scene::~scene ()
    {
        // free memory used by the objects
        std::vector<object*>::const_iterator cur = this->m_objects.begin ();
        std::vector<object*>::const_iterator end = this->m_objects.end ();

        for (; cur != end; cur ++)
        {
            delete *cur;
        }

        // remove elements from the vector
        this->m_objects.erase (this->m_objects.begin (), this->m_objects.end ());

        // free camera
        delete this->m_camera;
    }

    camera* scene::getCamera ()
    {
        return this->m_camera;
    }

    bool scene::isOrthogonal ()
    {
        return this->m_isOrthogonal;
    }

    float scene::getProjectionWidth ()
    {
        return this->m_width;
    }

    float scene::getProjectionHeight ()
    {
        return this->m_height;
    }

    void scene::render ()
    {
        std::vector<object*>::const_iterator cur = this->m_objects.begin ();
        std::vector<object*>::const_iterator end = this->m_objects.end ();

        for (; cur != end; cur ++)
        {
            (*cur)->render ();
        }
    }
}