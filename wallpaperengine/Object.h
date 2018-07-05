#ifndef WALLENGINE_OBJECT_H
#define WALLENGINE_OBJECT_H

#include <iostream>
#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Object
{
public:
    Object (json json_data);

private:
    int m_id;

    bool m_visible;

    std::string m_model;
    std::string m_angles;
    std::string m_image;
};


#endif //WALLENGINE_OBJECT_H
