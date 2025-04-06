#include "CEffect.h"

#include <iostream>
#include <utility>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h"
#include "WallpaperEngine/Core/Wallpapers/CScene.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::UserSettings;

CEffect::CEffect (std::string name, std::string description, std::string group, std::string preview, CObject* object,
                  CUserSettingBoolean* visible) :
    m_name (std::move (name)),
    m_description (std::move (description)),
    m_group (std::move (group)),
    m_preview (std::move (preview)),
    m_object (object),
    m_visible (visible) {}

CEffect* CEffect::fromJSON (json data, CUserSettingBoolean* visible, CObject* object, CContainer* container) {
    auto file_it = jsonFindRequired (data, "file", "Object effect must have a file");
    auto effectpasses_it = data.find ("passes");

    json content = json::parse (container->readFileAsString(file_it->get<std::string> ()));

    auto name_it = jsonFindRequired (content, "name", "Effect must have a name");
    auto description = jsonFindDefault<std::string> (content, "description", "");
    auto group_it = jsonFindRequired (content, "group", "Effect must have a group");
    auto preview = jsonFindDefault<std::string> (content, "preview", "");
    auto passes_it = jsonFindRequired (content, "passes", "Effect must have a pass list");
    auto dependencies_it = jsonFindRequired (content, "dependencies", "");
    auto fbos_it = content.find ("fbos");

    auto* effect = new CEffect (*name_it, description, *group_it, preview, object, visible);

    CEffect::materialsFromJSON (passes_it, effect, container);
    CEffect::dependencyFromJSON (dependencies_it, effect);

    if (fbos_it != content.end ())
        CEffect::fbosFromJSON (fbos_it, effect);

    if (effectpasses_it != data.end ()) {
        auto cur = effectpasses_it->begin ();
        auto end = effectpasses_it->end ();

        for (int passNumber = 0; cur != end; ++cur, passNumber++) {
            auto constants_it = cur->find ("constantshadervalues");
            auto combos_it = cur->find ("combos");
            auto textures_it = cur->find ("textures");

            if (constants_it == cur->end () && combos_it == cur->end () && textures_it == cur->end ())
                continue;

            Images::CMaterial* material = effect->getMaterials ().at (passNumber);

            for (const auto& passCur : material->getPasses ()) {
                if (textures_it != cur->end ()) {
                    std::vector<std::string>::size_type textureNumber = 0;

                    for (const auto& texturesCur : (*textures_it)) {
                        std::string texture;

                        if (texturesCur.is_null ()) {
                            if (textureNumber == 0) {
                                auto* image = object->as<CImage> ();

                                auto passTextures = (*image->getMaterial ()->getPasses ().begin ())->getTextures ();

                                if (passTextures.empty ()) {
                                    // TODO: SET CHECKERBOARD TEXTURE AS DEFAULT IN THESE SITUATIONS
                                    texture = "";
                                } else {
                                    texture = *passTextures.begin ();
                                }
                            } else {
                                texture = "";
                            }
                        } else {
                            texture = texturesCur;
                        }

                        const auto& passTextures = passCur->getTextures ();

                        if (textureNumber < passTextures.size ())
                            passCur->setTexture (textureNumber, texture);
                        else
                            passCur->insertTexture (texture);

                        textureNumber++;
                    }
                }

                if (combos_it != cur->end ()) {
                    CEffect::combosFromJSON (combos_it, passCur);
                }

                if (constants_it != cur->end ()) {
                    CEffect::constantsFromJSON (constants_it, passCur);
                }
            }
        }
    }

    return effect;
}

void CEffect::combosFromJSON (const json::const_iterator& combos_it, Core::Objects::Images::Materials::CPass* pass) {
    for (const auto& cur : combos_it->items ())
        pass->insertCombo (cur.key (), cur.value ());
}

void CEffect::constantsFromJSON (const json::const_iterator& constants_it,
                                 Core::Objects::Images::Materials::CPass* pass) {
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

        pass->insertConstant (cur.key (), constant);
    }
}

void CEffect::fbosFromJSON (const json::const_iterator& fbos_it, CEffect* effect) {
    for (const auto& cur : (*fbos_it))
        effect->insertFBO (Effects::CFBO::fromJSON (cur));
}

void CEffect::dependencyFromJSON (const json::const_iterator& dependencies_it, CEffect* effect) {
    for (const auto& cur : (*dependencies_it))
        effect->insertDependency (cur);
}

void CEffect::materialsFromJSON (const json::const_iterator& passes_it, CEffect* effect, CContainer* container) {
    for (const auto& cur : (*passes_it)) {
        auto materialfile = cur.find ("material");
        auto target = cur.find ("target");
        auto bind = cur.find ("bind");

        if (materialfile == cur.end ())
            sLog.exception ("Found an effect ", effect->m_name, " without material");

        Images::CMaterial* material;

        if (target == cur.end ())
            material = Images::CMaterial::fromFile (materialfile->get<std::string> (), container);
        else
            material = Images::CMaterial::fromFile (materialfile->get<std::string> (), *target, container);

        if (bind != cur.end ()) {
            for (const auto& bindCur : (*bind))
                material->insertTextureBind (Effects::CBind::fromJSON (bindCur));
        }

        effect->insertMaterial (material);
    }
}

const std::vector<std::string>& CEffect::getDependencies () const {
    return this->m_dependencies;
}

const std::vector<Images::CMaterial*>& CEffect::getMaterials () const {
    return this->m_materials;
}

const std::vector<Effects::CFBO*>& CEffect::getFbos () const {
    return this->m_fbos;
}

bool CEffect::isVisible () const {
    return this->m_visible->processValue (this->m_object->getScene ()->getProject ().getProperties ());
}

Effects::CFBO* CEffect::findFBO (const std::string& name) {
    for (const auto& cur : this->m_fbos)
        if (cur->getName () == name)
            return cur;

    sLog.exception ("cannot find fbo ", name);
}

void CEffect::insertDependency (const std::string& dep) {
    this->m_dependencies.push_back (dep);
}

void CEffect::insertMaterial (Images::CMaterial* material) {
    this->m_materials.push_back (material);
}

void CEffect::insertFBO (Effects::CFBO* fbo) {
    this->m_fbos.push_back (fbo);
}