#include "CMaterial.h"

#include "WallpaperEngine/Core/Objects/Images/Materials/CPass.h"
#include "WallpaperEngine/Logging/CLog.h"
#include <nlohmann/json.hpp>
#include <utility>

using namespace WallpaperEngine::Assets;

using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::Objects::Images;

CMaterial::CMaterial (
    std::string name, bool solidlayer, std::map<int, const Effects::CBind*> textureBindings,
    std::vector<const Materials::CPass*> passes
) :
    m_name (std::move(name)),
    m_textureBindings (std::move(textureBindings)),
    m_passes (std::move(passes)),
    m_solidlayer (solidlayer) {}
CMaterial::CMaterial (
    std::string name, std::string target, bool solidlayer,
    std::map<int, const Effects::CBind*> textureBindings, std::vector<const Materials::CPass*> passes
) :
    m_name (std::move(name)),
    m_target (std::move(target)),
    m_textureBindings (std::move(textureBindings)),
    m_passes (std::move(passes)),
    m_solidlayer (solidlayer)  {}

const CMaterial* CMaterial::fromFile (
    const std::filesystem::path& filename, const CContainer* container, bool solidlayer,
    std::map<int, const Effects::CBind*> textureBindings, const OverrideInfo* overrides
) {
    return fromJSON (filename, json::parse (container->readFileAsString (filename)), solidlayer, std::move(textureBindings), overrides);
}

const CMaterial* CMaterial::fromFile (
    const std::filesystem::path& filename, const std::string& target, const CContainer* container, bool solidlayer,
    std::map<int, const Effects::CBind*> textureBindings, const OverrideInfo* overrides
) {
    return fromJSON (filename, json::parse (container->readFileAsString (filename)), target, solidlayer, std::move(textureBindings), overrides);
}

const CMaterial* CMaterial::fromJSON (
    const std::string& name, const json& data, const std::string& target, bool solidlayer,
    std::map<int, const Effects::CBind*> textureBindings, const OverrideInfo* overrides
) {
    const auto passes_it = jsonFindRequired (data, "passes", "Material must have at least one pass");
    std::vector<const Materials::CPass*> passes;

    for (const auto& cur : (*passes_it))
        passes.push_back (Materials::CPass::fromJSON (cur, overrides));

    return new CMaterial (name, target, solidlayer, std::move(textureBindings), passes);
}

const CMaterial* CMaterial::fromJSON (
    const std::string& name, const json& data, bool solidlayer, std::map<int, const Effects::CBind*> textureBindings,
    const OverrideInfo* overrides
) {
    const auto passes_it = jsonFindRequired (data, "passes", "Material must have at least one pass");
    std::vector<const Materials::CPass*> passes;

    for (const auto& cur : (*passes_it))
        passes.push_back (Materials::CPass::fromJSON (cur, overrides));

    return new CMaterial (name, solidlayer, std::move(textureBindings), passes);
}

const CMaterial* CMaterial::fromCommand (const json& data) {
    const std::string& command = jsonFindRequired <std::string> (data, "command", "Command material must have a command");
    const std::string& target = jsonFindRequired <std::string> (data, "target", "Command material must have a target");
    const std::string& source = jsonFindRequired <std::string> (data, "source", "Command material must have a source");
    std::vector<const Materials::CPass*> passes;

    if (command == "copy") {
        passes.push_back (
            Materials::CPass::fromJSON ({
                {"blending", "normal"},
                {"cullmode", "nocull"},
                {"depthtest", "disabled"},
                {"depthwrite", "disabled"},
                {"shader", "commands/copy"},
                {"textures", json::array ({source, target})}
            }, nullptr)
        );
    } else if (command == "swap") {
        // TODO: HOW TO IMPLEMENT THIS ONE?
        sLog.exception ("Command material swap not implemented yet");
    } else {
        sLog.exception ("Unknown command: ", command);
    }

    return new CMaterial (command, false, {}, passes);
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

bool CMaterial::isSolidLayer () const {
    return this->m_solidlayer;
}