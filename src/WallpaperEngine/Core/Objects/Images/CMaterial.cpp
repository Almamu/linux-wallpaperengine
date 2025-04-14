#include "CMaterial.h"

#include "WallpaperEngine/Core/Objects/Images/Materials/CPass.h"
#include <nlohmann/json.hpp>

using namespace WallpaperEngine::Assets;

using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::Objects::Images;

CMaterial::CMaterial (
    std::string name, std::map<int, const Effects::CBind*> textureBindings,
    std::vector<const Materials::CPass*> passes
) :
    m_name (name),
    m_textureBindings (textureBindings),
    m_passes (passes) {}
CMaterial::CMaterial (
    std::string name, std::string target,
    std::map<int, const Effects::CBind*> textureBindings, std::vector<const Materials::CPass*> passes
) :
    m_name (name),
    m_target (target),
    m_textureBindings (textureBindings),
    m_passes (passes) {}

const CMaterial* CMaterial::fromFile (
    const std::filesystem::path& filename, const CContainer* container,
    std::map<int, const Effects::CBind*> textureBindings, const OverrideInfo* overrides
) {
    return fromJSON (filename, json::parse (container->readFileAsString (filename)), textureBindings, overrides);
}

const CMaterial* CMaterial::fromFile (
    const std::filesystem::path& filename, const std::string& target,
    const CContainer* container, std::map<int, const Effects::CBind*> textureBindings, const OverrideInfo* overrides
) {
    return fromJSON (filename, json::parse (container->readFileAsString (filename)), target, textureBindings, overrides);
}

const CMaterial* CMaterial::fromJSON (
    const std::string& name, const json& data, const std::string& target,
    std::map<int, const Effects::CBind*> textureBindings, const OverrideInfo* overrides
) {
    const auto passes_it = jsonFindRequired (data, "passes", "Material must have at least one pass");
    std::vector<const Materials::CPass*> passes;

    for (const auto& cur : (*passes_it))
        passes.push_back (Materials::CPass::fromJSON (cur, overrides));

    return new CMaterial (name, target, textureBindings, passes);
}

const CMaterial* CMaterial::fromJSON (
    const std::string& name, const json& data, std::map<int, const Effects::CBind*> textureBindings,
    const OverrideInfo* overrides
) {
    const auto passes_it = jsonFindRequired (data, "passes", "Material must have at least one pass");
    std::vector<const Materials::CPass*> passes;

    for (const auto& cur : (*passes_it))
        passes.push_back (Materials::CPass::fromJSON (cur, overrides));

    return new CMaterial (name, textureBindings, passes);
}

const std::vector<const Materials::CPass*>& CMaterial::getPasses () const {
    return this->m_passes;
}

const std::map<int, const Effects::CBind*>& CMaterial::getTextureBinds () const {
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