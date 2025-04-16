#include "CEffect.h"

#include <utility>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/Images/Materials/CPass.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::UserSettings;

CEffect::CEffect (
    std::string name, std::string description, std::string group, std::string preview, const CProject& project,
    const CUserSettingBoolean* visible, std::vector<std::string> dependencies, std::vector<const Effects::CFBO*> fbos,
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
    const json& data, const CUserSettingBoolean* visible, const CProject& project, const Images::CMaterial* material,
    const CContainer* container
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
        overrides = overridesFromJSON (effectpasses_it, material);
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
        std::string uppercase = std::string (cur.key ());

        std::transform (uppercase.begin (), uppercase.end (), uppercase.begin (), ::toupper);
        combos.insert (std::pair (uppercase, cur.value ()));
    }

    return combos;
}

std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> CEffect::constantsFromJSON (
    const json::const_iterator& constants_it
) {
    std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> constants;

    for (auto& cur : constants_it->items ()) {
        auto val = cur.value ();

        Effects::Constants::CShaderConstant* constant;

        // if the constant is an object, that means the constant has some extra information
        // for the UI, take the value, which is what we need

        // TODO: SUPPORT USER SETTINGS HERE
        if (cur.value ().is_object ()) {
            auto it = cur.value ().find ("value");

            if (it == cur.value ().end ()) {
                sLog.error ("Found object for shader constant without \"value\" member");
                continue;
            }

            val = it.value ();
        }

        if (val.is_number_float ()) {
            constant = new Effects::Constants::CShaderConstantFloat (val.get<float> ());
        } else if (val.is_number_integer ()) {
            constant = new Effects::Constants::CShaderConstantInteger (val.get<int> ());
        } else if (val.is_string ()) {
            // try a vector 4 first, then a vector3 and then a vector 2
            constant = new Effects::Constants::CShaderConstantVector4 (WallpaperEngine::Core::aToVector4 (val));
        } else {
            sLog.exception ("unknown shader constant type ", val);
        }

        constants.insert (std::pair (cur.key (), constant));
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
    const json::const_iterator& passes_it, const std::string& name, const CContainer* container,
    std::map<int, Images::CMaterial::OverrideInfo> overrides
) {
    std::vector<const Images::CMaterial*> materials;

    int materialNumber = -1;
    for (const auto& cur : (*passes_it)) {
        ++materialNumber;
        const auto materialfile = cur.find ("material");
        const auto target = cur.find ("target");
        const auto bind_it = cur.find ("bind");

        if (materialfile == cur.end ())
            sLog.exception ("Found an effect ", name, " without material");

        std::map<int, const Effects::CBind*> textureBindings;

        if (bind_it != cur.end ()) {
            for (const auto& bindCur : (*bind_it)) {
                const auto* bind = Effects::CBind::fromJSON (bindCur);
                textureBindings.insert (std::pair (bind->getIndex (), bind));
            }
        }

        const Images::CMaterial* material;
        const Images::CMaterial::OverrideInfo* overrideInfo;
        const auto overrideIt = overrides.find (materialNumber);

        if (overrideIt != overrides.end ()) {
            overrideInfo = &overrideIt->second;
        }

        if (target == cur.end ())
            material = Images::CMaterial::fromFile (materialfile->get<std::string> (), container, textureBindings, overrideInfo);
        else
            material = Images::CMaterial::fromFile (materialfile->get<std::string> (), *target, container, textureBindings, overrideInfo);

        materials.push_back (material);
    }

    return materials;
}

std::map<int, Images::CMaterial::OverrideInfo> CEffect::overridesFromJSON (
    const json::const_iterator& passes_it, const Images::CMaterial* material
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
            override.constants = CEffect::constantsFromJSON (constants_it);
        }

        if (textures_it != cur.end ()) {
            // TODO: MAYBE CHANGE THIS TO BE SOMEWHERE ELSE? THIS IS REALLY MODIFYING THE DATA
            //  BUT IT'S USEFUL TO HAVE TO SIMPLIFY RENDERING CODE
            for (const auto& texture : (*textures_it)) {
                ++textureNumber;
                std::string name;

                if (texture.is_null ()) {
                    if (textureNumber == 0) {
                        auto passTextures = (*material->getPasses ().begin ())->getTextures ();

                        if (passTextures.empty ()) {
                            // TODO: SET CHECKERBOARD TEXTURE AS DEFAULT IN THESE SITUATIONS
                            name = "";
                        } else {
                            name = passTextures.begin ()->second;
                        }
                    } else {
                        name = "";
                    }
                } else {
                    name = texture;
                }

                override.textures.insert (std::pair (textureNumber, name));
            }
        }

        result.insert (std::pair (materialNumber, override));
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
    return this->m_project;
}

bool CEffect::isVisible () const {
    return this->m_visible->processValue (this->getProject ().getProperties ());
}

const Effects::CFBO* CEffect::findFBO (const std::string& name) {
    for (const auto& cur : this->m_fbos)
        if (cur->getName () == name)
            return cur;

    sLog.exception ("cannot find fbo ", name);
}
