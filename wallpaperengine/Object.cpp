#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Object.h"

using json = nlohmann::json;

Object::Object (json json_data)
{
    json::const_iterator angles = json_data.find ("angles");
    json::const_iterator image = json_data.find ("image");
    json::const_iterator id = json_data.find ("id");
    json::const_iterator visible = json_data.find ("visible");
    json::const_iterator model = json_data.find ("model");

    if (angles != json_data.end () && angles.value ().is_null() == false)
    {
        this->m_angles = json_data ["angles"];
    }

    if (image != json_data.end () && image.value ().is_null() == false)
    {
        this->m_image = json_data ["image"];
    }

    if (id != json_data.end () && id.value ().is_null() == false)
    {
        this->m_id = json_data ["id"];
    }

    if (visible != json_data.end () && visible.value ().is_null() == false)
    {
        this->m_visible = json_data ["visible"];
    }
    else
    {
        this->m_visible = true;
    }

    if (model != json_data.end () && model.value ().is_null() == false)
    {
        this->m_model = json_data ["model"];
    }
}
