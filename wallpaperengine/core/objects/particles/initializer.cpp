#include "initializer.h"

#include "initializers/lifetimerandom.h"
#include "initializers/sizerandom.h"
#include "initializers/rotationrandom.h"
#include "initializers/velocityrandom.h"
#include "initializers/colorrandom.h"
#include "initializers/alpharandom.h"

using namespace wp::core::objects::particles;

initializer* initializer::fromJSON (json data)
{
    json::const_iterator id_it = data.find ("id");
    json::const_iterator name_it = data.find ("name");

    if (id_it == data.end ())
    {
        throw std::runtime_error ("Particle's initializer must have an id");
    }

    if (name_it == data.end ())
    {
        throw std::runtime_error ("Particle's initializer must have a name");
    }

    if (*name_it == "lifetimerandom")
    {
        return initializers::lifetimerandom::fromJSON (data, *id_it);
    }
    else if (*name_it == "sizerandom")
    {
        return initializers::sizerandom::fromJSON (data, *id_it);
    }
    else if (*name_it == "rotationrandom")
    {
        return initializers::rotationrandom::fromJSON (data, *id_it);
    }
    else if (*name_it == "velocityrandom")
    {
        return initializers::velocityrandom::fromJSON (data, *id_it);
    }
    else if (*name_it == "colorrandom")
    {
        return initializers::colorrandom::fromJSON (data, *id_it);
    }
    else if (*name_it == "alpharandom")
    {
        return initializers::alpharandom::fromJSON (data, *id_it);
    }
    else
    {
        throw std::runtime_error ("Particle's got an unknown initializer");
    }
}


initializer::initializer (irr::u32 id, std::string name) :
    m_id (id),
    m_name (std::move(name))
{
}


std::string& initializer::getName ()
{
    return this->m_name;
}

irr::u32 initializer::getId ()
{
    return this->m_id;
}