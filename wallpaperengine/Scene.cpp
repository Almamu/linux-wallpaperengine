#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Scene.h"

using json = nlohmann::json;

Scene::Scene(irr::io::path file)
{
    std::ifstream _in (file.c_str ());
    this->m_content = "";
    this->m_content.append (std::istreambuf_iterator<char> (_in), std::istreambuf_iterator<char> ());
    this->m_json = json::parse (this->m_content);

    // check basic elements
    json::const_iterator camera_it =    this->m_json.find ("camera");
    json::const_iterator general_it =   this->m_json.find ("general");
    json::const_iterator objects_it =   this->m_json.find ("objects");

    if (objects_it != this->m_json.end () && objects_it.value ().is_array() == true)
    {
        json::const_iterator cur = this->m_json ["objects"].begin ();
        json::const_iterator end = this->m_json ["objects"].end ();

        for (; cur != end; cur ++)
        {
            this->m_objects.push_back (new Object (*cur));
        }
    }
}