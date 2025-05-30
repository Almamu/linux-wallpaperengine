#include "CEffect.h"

#include <utility>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/Images/Materials/CPass.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector2.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantProperty.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::UserSettings;

CEffect::CEffect (
    std::string name, std::string description, std::string group, std::string preview,
    std::shared_ptr <const Core::CProject> project, const CUserSettingBoolean* visible,
    std::vector<std::string> dependencies, std::vector<const Effects::CFBO*> fbos,
    std::vector<const Images::CMaterial*> materials
) :
    m_name (std::move(name)),
    m_description (std::move(description)),
    m_group (std::move(group)),
    m_preview (std::move(preview)),
    m_visible (visible),
    m_dependencies (std::move(dependencies)),
    m_fbos (std::move(fbos)),
    m_project (project),
    m_materials (std::move(materials)) {}

const CEffect* CEffect::fromJSON (
    const json& data, const CUserSettingBoolean* visible, std::shared_ptr <const Core::CProject> project,
    const Images::CMaterial* material, const std::shared_ptr<const CContainer>& container
) {
    const auto file = jsonFindRequired <std::string> (data, "file", "Object effect must have a file");
    const auto effectpasses_it = data.find ("passes");

    json content = json::parse (container->readFileAsString(file));

    const auto effectName = jsonFindRequired <std::string> (content, "name", "Effect must have a name");
    const auto passes_it = jsonFindRequired (content, "passes", "Effect must have a pass list");
    const auto fbos_it = content.find ("fbos");

    // info to override in the pass information, used by material generation
    std::map<int, Images::CMaterial::OverrideInfo> overrides;
    std::vector<const Effects::CFBO*> fbos;

    if (fbos_it != content.end ())
        fbos = CEffect::fbosFromJSON (fbos_it);

    if (effectpasses_it != data.end ()) {
        overrides = overridesFromJSON (effectpasses_it, material, project);
    }

    return new CEffect (
        effectName,
        jsonFindDefault<std::string> (content, "description", ""),
        jsonFindRequired <std::string> (content, "group", "Effect must have a group"),
        jsonFindDefault<std::string> (content, "preview", ""),
        project,
        visible,
        dependenciesFromJSON (jsonFindRequired (content, "dependencies", "")),
        fbos,
        materialsFromJSON (passes_it, effectName, container, overrides)
    );
}

std::map<std::string, int> CEffect::combosFromJSON (const json::const_iterator& combos_it) {
    std::map<std::string, int> combos;

    for (const auto& cur : combos_it->items ()) {
        auto uppercase = std::string (cur.key ());

        std::transform (uppercase.begin (), uppercase.end (), uppercase.begin (), ::toupper);
        combos.emplace (uppercase, cur.value ());
    }

    return combos;
}

std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> CEffect::constantsFromJSON (
    const json::const_iterator& constants_it, std::shared_ptr <const Core::CProject> project
) {
    std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> constants;

    for (auto& cur : constants_it->items ()) {
        auto val = cur.value ();

        Effects::Constants::CShaderConstant* constant = nullptr;

        // if the constant is an object, that means the constant has some extra information
        // for the UI, take the value, which is what we need

        if (cur.value ().is_object ()) {
            auto user = cur.value ().find ("user");
            auto it = cur.value ().find ("value");

            if (user == cur.value ().end () && it == cur.value ().end ()) {
                sLog.error (R"(Found object for shader constant without "value" and "user" setting)");
                continue;
            }

            if (user != cur.value ().end () && user->is_string ()) {
                // look for a property with the correct name
                const auto& properties = project->getProperties ();
                const auto property = properties.find (*user);

                if (property != properties.end ()) {
                    constant = new Effects::Constants::CShaderConstantProperty (property->second);
                } else {
                    sLog.error ("Shader constant pointing to non-existant project property: ", user->get <std::string> ());
                    val = it.value ();
                }
            } else {
                val = it.value ();
            }
        }

        // TODO: REFACTOR THIS SO IT'S NOT SO DEEP INTO THE FUNCTION
        if (constant == nullptr) {
            if (val.is_number_float ()) {
                constant = new Effects::Constants::CShaderConstantFloat (val.get<float> ());
            } else if (val.is_number_integer ()) {
                constant = new Effects::Constants::CShaderConstantInteger (val.get<int> ());
            } else if (val.is_string ()) {
                // count the amount of spaces to determine which type of vector we have
                std::string value = val;

                size_t spaces =
                    std::count_if (value.begin (), value.end (), [&] (const auto& item) { return item == ' '; });

                if (spaces == 1) {
                    constant =
                        new Effects::Constants::CShaderConstantVector2 (WallpaperEngine::Core::aToVector2 (value));
                } else if (spaces == 2) {
                    constant =
                        new Effects::Constants::CShaderConstantVector3 (WallpaperEngine::Core::aToVector3 (value));
                } else if (spaces == 3) {
                    constant =
                        new Effects::Constants::CShaderConstantVector4 (WallpaperEngine::Core::aToVector4 (value));
                } else {
                    sLog.exception ("unknown shader constant type ", value);
                }
            } else {
                sLog.exception ("unknown shader constant type ", val);
            }
        }

        constants.emplace (cur.key (), constant);
    }

    return constants;
}

std::vector<const Effects::CFBO*> CEffect::fbosFromJSON (const json::const_iterator& fbos_it) {
    std::vector<const Effects::CFBO*> fbos;

    for (const auto& cur : (*fbos_it))
        fbos.push_back (Effects::CFBO::fromJSON (cur));

    return fbos;
}

std::vector<std::string> CEffect::dependenciesFromJSON (const json::const_iterator& dependencies_it) {
    std::vector<std::string> dependencies;

    for (const auto& cur : (*dependencies_it))
        dependencies.push_back (cur);

    return dependencies;
}

std::vector<const Images::CMaterial*> CEffect::materialsFromJSON (
    const json::const_iterator& passes_it, const std::string& name, const std::shared_ptr<const CContainer>& container,
    std::map<int, Images::CMaterial::OverrideInfo> overrides
) {
    std::vector<const Images::CMaterial*> materials;

    int materialNumber = -1;
    for (const auto& cur : (*passes_it)) {
        ++materialNumber;
        const auto materialfile = cur.find ("material");
        const auto target_it = cur.find ("target");
        const auto bind_it = cur.find ("bind");
        const auto command_it = cur.find ("command");
        const auto compose_it = cur.find ("compose");
        const Images::CMaterial* material = nullptr;

        if (compose_it != cur.end ()) {
            sLog.error ("Composing materials is not supported yet...");
        }

        if (materialfile != cur.end ()) {
            std::map<int, const Effects::CBind*> textureBindings;

            if (bind_it != cur.end ()) {
                for (const auto& bindCur : (*bind_it)) {
                    const auto* bind = Effects::CBind::fromJSON (bindCur);
                    textureBindings.emplace (bind->getIndex (), bind);
                }
            }

            const Images::CMaterial::OverrideInfo* overrideInfo = nullptr;
            const auto overrideIt = overrides.find (materialNumber);

            if (overrideIt != overrides.end ()) {
                overrideInfo = &overrideIt->second;
            }

            if (target_it == cur.end ()) {
                material = Images::CMaterial::fromFile (
                    materialfile->get<std::string> (), container, false, textureBindings, overrideInfo);
            } else {
                material = Images::CMaterial::fromFile (
                    materialfile->get<std::string> (), *target_it, container, false, textureBindings, overrideInfo);
            }
        } else if (command_it != cur.end ()) {
            material = Images::CMaterial::fromCommand (cur);
        } else {
            sLog.exception ("Material without command nor material file: ", name, " (", materialNumber,")");
        }

        materials.push_back (material);
    }

    return materials;
}

std::map<int, Images::CMaterial::OverrideInfo> CEffect::overridesFromJSON (
    const json::const_iterator& passes_it, const Images::CMaterial* material,
    std::shared_ptr <const Core::CProject> project
) {
    std::map<int, Images::CMaterial::OverrideInfo> result;

    int materialNumber = -1;
    for (const auto& cur : (*passes_it)) {
        ++materialNumber;
        auto constants_it = cur.find ("constantshadervalues");
        auto combos_it = cur.find ("combos");
        auto textures_it = cur.find ("textures");
        Images::CMaterial::OverrideInfo override;
        int textureNumber = -1;

        if (combos_it != cur.end ()) {
            override.combos = CEffect::combosFromJSON (combos_it);
        }

        if (constants_it != cur.end ()) {
            override.constants = CEffect::constantsFromJSON (constants_it, project);
        }

        if (textures_it != cur.end ()) {
            // TODO: MAYBE CHANGE THIS TO BE SOMEWHERE ELSE? THIS IS REALLY MODIFYING THE DATA
            //  BUT IT'S USEFUL TO HAVE TO SIMPLIFY RENDERING CODE
            for (const auto& texture : (*textures_it)) {
                ++textureNumber;
                std::string name;

                if (texture.is_null () && textureNumber > 0) {
                    continue;
                }

                if (textureNumber == 0) {
                    auto passTextures = (*material->getPasses ().begin ())->getTextures ();

                    if (passTextures.empty()) {
                        continue;
                    } else {
                        name = passTextures.begin ()->second;
                    }
                } else {
                    name = texture;
                }

                override.textures.emplace (textureNumber, name);
            }
        }

        result.emplace (materialNumber, override);
    }

    return result;
}

const std::vector<std::string>& CEffect::getDependencies () const {
    return this->m_dependencies;
}

const std::vector<const Images::CMaterial*>& CEffect::getMaterials () const {
    return this->m_materials;
}

const std::vector<const Effects::CFBO*>& CEffect::getFbos () const {
    return this->m_fbos;
}

const Core::CProject& CEffect::getProject () const {
    return *this->m_project;
}

bool CEffect::isVisible () const {
    return this->m_visible->getBool ();
}

const Effects::CFBO* CEffect::findFBO (const std::string& name) {
    for (const auto& cur : this->m_fbos)
        if (cur->getName () == name)
            return cur;

    sLog.exception ("cannot find fbo ", name);
}
