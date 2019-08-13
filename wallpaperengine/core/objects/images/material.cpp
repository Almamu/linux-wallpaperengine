#include "material.h"

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "../../../fs/utils.h"

using namespace wp::core::objects::images;

material::material ()
{
}

material* material::fromFile (irr::io::path filename)
{
    json content = json::parse (wp::fs::utils::loadFullFile (filename));

    json::const_iterator passes_it = content.find ("passes");

    if (passes_it == content.end ())
    {
        throw std::runtime_error ("Material must have at least one pass");
    }

    material* material = new class material ();

    json::const_iterator cur = (*passes_it).begin ();
    json::const_iterator end = (*passes_it).end ();

    for (; cur != end; cur ++)
    {
        material->insertPass (
            materials::passes::fromJSON (*cur)
        );
    }

    return material;
}

void material::insertPass (materials::passes* mass)
{
    this->m_passes.push_back (mass);
}

std::vector <materials::passes*>* material::getPasses ()
{
    return &this->m_passes;
}