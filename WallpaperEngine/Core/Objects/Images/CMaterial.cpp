#include "CMaterial.h"

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "../../../fs/utils.h"

using namespace WallpaperEngine::Core::Objects::Images;

CMaterial::CMaterial ()
{
}

CMaterial* CMaterial::fromFile (irr::io::path filename)
{
    return fromJSON (
        json::parse (WallpaperEngine::FileSystem::loadFullFile (filename))
    );
}

CMaterial* CMaterial::fromJSON (json data)
{
    json::const_iterator passes_it = data.find ("passes");

    if (passes_it == data.end ())
    {
        throw std::runtime_error ("Material must have at least one pass");
    }

    CMaterial* material = new CMaterial ();

    json::const_iterator cur = (*passes_it).begin ();
    json::const_iterator end = (*passes_it).end ();

    for (; cur != end; cur ++)
    {
        material->insertPass (
                Materials::CPassess::fromJSON (*cur)
        );
    }

    return material;
}

void CMaterial::insertPass (Materials::CPassess* mass)
{
    this->m_passes.push_back (mass);
}

std::vector <Materials::CPassess*>* CMaterial::getPasses ()
{
    return &this->m_passes;
}