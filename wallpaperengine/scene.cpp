#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "scene.h"
#include "camera.h"

namespace wp
{
    using json = nlohmann::json;

    scene::scene (irr::io::path file)
    {
        std::ifstream _in (file.c_str ());
        this->m_content = "";
        this->m_content.append (std::istreambuf_iterator<char> (_in), std::istreambuf_iterator<char> ());
        this->m_json = json::parse (this->m_content);

        // check basic elements
        json::const_iterator camera_it =    this->m_json.find ("camera");
        json::const_iterator general_it =   this->m_json.find ("general");
        json::const_iterator objects_it =   this->m_json.find ("objects");

        if (camera_it != this->m_json.end () && objects_it.value ().is_array () == true)
        {
            this->m_camera = new camera (*camera_it);
        }

        if (objects_it != this->m_json.end () && objects_it.value ().is_array () == true)
        {
            json::const_iterator cur = this->m_json ["objects"].begin ();
            json::const_iterator end = this->m_json ["objects"].end ();

            for (; cur != end; cur ++)
            {
                this->m_objects.push_back (new object (*cur));
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
}