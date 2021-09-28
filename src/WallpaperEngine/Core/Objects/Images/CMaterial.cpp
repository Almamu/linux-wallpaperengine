#include "CMaterial.h"

#include <nlohmann/json.hpp>

#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Assets;

using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::Objects::Images;

CMaterial::CMaterial () :
    m_target ("")
{
}

CMaterial* CMaterial::fromFile (const std::string& filename, CContainer* container)
{
    return fromJSON (
        json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container))
    );
}
CMaterial* CMaterial::fromFile (const std::string& filename, const std::string& target, CContainer* container)
{
    return fromJSON (
        json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container)), target
    );
}

CMaterial* CMaterial::fromJSON (json data, const std::string& target)
{
    CMaterial* material = fromJSON (data);

    material->setTarget (target);

    return material;
}

CMaterial* CMaterial::fromJSON (json data)
{
    auto passes_it = jsonFindRequired (data, "passes", "Material must have at least one pass");

    CMaterial* material = new CMaterial ();

    auto cur = (*passes_it).begin ();
    auto end = (*passes_it).end ();

    for (; cur != end; cur ++)
    {
        material->insertPass (
            Materials::CPass::fromJSON (*cur)
        );
    }

    return material;
}

void CMaterial::insertPass (Materials::CPass* mass)
{
    this->m_passes.push_back (mass);
}

void CMaterial::insertTextureBind (Effects::CBind* bind)
{
    this->m_textureBindings.insert (std::make_pair (bind->getIndex (), bind));
}

void CMaterial::setTarget (const std::string& target)
{
    this->m_target = target;
}

const std::vector <Materials::CPass*>& CMaterial::getPasses () const
{
    return this->m_passes;
}
const std::map <int, Effects::CBind*>& CMaterial::getTextureBinds () const
{
    return this->m_textureBindings;
}

const std::string& CMaterial::getTarget () const
{
    return this->m_target;
}

const bool CMaterial::hasTarget () const
{
    return this->m_target.empty () == false;
}