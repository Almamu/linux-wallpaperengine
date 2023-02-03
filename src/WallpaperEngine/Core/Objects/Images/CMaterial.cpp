#include "CMaterial.h"

#include <nlohmann/json.hpp>

#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Assets;

using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::Objects::Images;

CMaterial::CMaterial (const std::string& name) :
    m_target (""),
    m_name (name)
{
}

CMaterial* CMaterial::fromFile (const std::string& filename, const CContainer* container)
{
    return fromJSON (
        filename, json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container))
    );
}
CMaterial* CMaterial::fromFile (const std::string& filename, const std::string& target, const CContainer* container)
{
    return fromJSON (
        filename, json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container)), target
    );
}

CMaterial* CMaterial::fromJSON (const std::string& name, json data, const std::string& target)
{
    CMaterial* material = fromJSON (name, data);

    material->setTarget (target);

    return material;
}

CMaterial* CMaterial::fromJSON (const std::string& name, json data)
{
    auto passes_it = jsonFindRequired (data, "passes", "Material must have at least one pass");

    CMaterial* material = new CMaterial (name);

    for (const auto& cur : (*passes_it))
        material->insertPass (Materials::CPass::fromJSON (cur));

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

const std::string& CMaterial::getName () const
{
    return this->m_name;
}

const bool CMaterial::hasTarget () const
{
    return this->m_target.empty () == false;
}