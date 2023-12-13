#include "CMaterial.h"

#include <nlohmann/json.hpp>
#include <utility>

#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Assets;

using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::Objects::Images;

CMaterial::CMaterial (std::string name) : m_name (std::move (name)) {}

CMaterial* CMaterial::fromFile (const std::string& filename, CContainer* container) {
    return fromJSON (filename, json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container)));
}

CMaterial* CMaterial::fromFile (const std::string& filename, const std::string& target, CContainer* container) {
    return fromJSON (filename, json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container)), target);
}

CMaterial* CMaterial::fromJSON (const std::string& name, json data, const std::string& target) {
    CMaterial* material = fromJSON (name, std::move (data));

    material->setTarget (target);

    return material;
}

CMaterial* CMaterial::fromJSON (const std::string& name, json data) {
    const auto passes_it = jsonFindRequired (data, "passes", "Material must have at least one pass");

    auto* material = new CMaterial (name);

    for (const auto& cur : (*passes_it))
        material->insertPass (Materials::CPass::fromJSON (cur));

    return material;
}

void CMaterial::insertPass (Materials::CPass* pass) {
    this->m_passes.push_back (pass);
}

void CMaterial::insertTextureBind (Effects::CBind* bind) {
    this->m_textureBindings.insert (std::make_pair (bind->getIndex (), bind));
}

void CMaterial::setTarget (const std::string& target) {
    this->m_target = target;
}

const std::vector<Materials::CPass*>& CMaterial::getPasses () const {
    return this->m_passes;
}

const std::map<int, Effects::CBind*>& CMaterial::getTextureBinds () const {
    return this->m_textureBindings;
}

const std::string& CMaterial::getTarget () const {
    return this->m_target;
}

const std::string& CMaterial::getName () const {
    return this->m_name;
}

bool CMaterial::hasTarget () const {
    return !this->m_target.empty ();
}