#include "CEffect.h"

#include <utility>

#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"

#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Core::Objects;

CEffect::CEffect (
        std::string name,
        std::string description,
        std::string group,
        std::string preview,
        Core::CObject* object):
    m_name (std::move(name)),
    m_description (std::move(description)),
    m_group (std::move(group)),
    m_preview (std::move(preview)),
    m_object (object)
{
}

CEffect* CEffect::fromJSON (json data, Core::CObject* object, CContainer* container)
{
    auto file_it = jsonFindRequired (data, "file", "Object effect must have a file");
    auto effectpasses_it = data.find ("passes");

    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile ((*file_it).get <std::string> (), container));

    auto name_it = jsonFindRequired (content, "name", "Effect must have a name");
    auto description_it = jsonFindRequired (content, "description", "Effect must have a description");
    auto group_it = jsonFindRequired (content, "group", "Effect must have a group");
    auto preview_it = jsonFindRequired (content, "preview", "Effect must have a preview");
    auto passes_it = jsonFindRequired (content, "passes", "Effect must have a pass list");
    auto dependencies_it = jsonFindRequired (content, "dependencies", "");
    auto fbos_it = content.find ("fbos");

    CEffect* effect = new CEffect (
        *name_it,
        *description_it,
        *group_it,
        *preview_it,
        object
    );

    CEffect::materialsFromJSON (passes_it, effect, container);
    CEffect::dependencyFromJSON (dependencies_it, effect);

    if (fbos_it != content.end ())
    {
        CEffect::fbosFromJSON (fbos_it, effect);
    }

    if (effectpasses_it != data.end ())
    {
        auto cur = (*effectpasses_it).begin ();
        auto end = (*effectpasses_it).end ();

        for (int passNumber = 0; cur != end; cur ++, passNumber ++)
        {
            auto constants_it = (*cur).find ("constantshadervalues");
            auto combos_it = (*cur).find ("combos");
            auto textures_it = (*cur).find ("textures");

            if (constants_it == (*cur).end () && combos_it == (*cur).end () && textures_it == (*cur).end ())
                continue;

            Images::CMaterial* material = effect->getMaterials ().at (passNumber);

            auto passCur = material->getPasses ().begin ();
            auto passEnd = material->getPasses ().end ();

            for (; passCur != passEnd; passCur ++)
            {
                if (textures_it != (*cur).end ())
                {
                    auto texturesCur = (*textures_it).begin ();
                    auto texturesEnd = (*textures_it).end ();

                    for (int textureNumber = 0; texturesCur != texturesEnd; texturesCur ++)
                    {
                        std::string texture;

                        if ((*texturesCur).is_null () == true)
                        {
                            if (object->is <CImage> () == false)
                            {
                                throw std::runtime_error ("unexpected null texture for non-image object");
                            }

                            CImage* image = object->as <CImage> ();

                            texture = (*(*image->getMaterial ()->getPasses ().begin ())->getTextures ().begin ());
                        }
                        else
                        {
                            texture = *texturesCur;
                        }

                        std::vector<std::string> passTextures = (*passCur)->getTextures ();

                        if (textureNumber < passTextures.size ())
                            (*passCur)->setTexture (textureNumber, texture);
                        else
                            (*passCur)->insertTexture (texture);

                        textureNumber ++;
                    }
                }

                if (combos_it != (*cur).end ())
                {
                    CEffect::combosFromJSON (combos_it, *passCur);
                }

                if (constants_it != (*cur).end ())
                {
                    CEffect::constantsFromJSON (constants_it, *passCur);
                }
            }
        }
    }

    return effect;
}

void CEffect::combosFromJSON (json::const_iterator combos_it, Core::Objects::Images::Materials::CPass* pass)
{
    auto cur = (*combos_it).begin ();
    auto end = (*combos_it).end ();

    for (; cur != end; cur ++)
    {
        pass->insertCombo (cur.key (), *cur);
    }
}

void CEffect::constantsFromJSON (json::const_iterator constants_it, Core::Objects::Images::Materials::CPass* pass)
{
    auto cur = (*constants_it).begin ();
    auto end = (*constants_it).end ();

    for (; cur != end; cur ++)
    {
        Effects::Constants::CShaderConstant* constant = nullptr;

        if ((*cur).is_number_float () == true)
        {
            constant = new Effects::Constants::CShaderConstantFloat ((*cur).get <float> ());
        }
        else if ((*cur).is_number_integer () == true)
        {
            constant = new Effects::Constants::CShaderConstantInteger ((*cur).get <irr::s32> ());
        }
        else if ((*cur).is_string () == true)
        {
            constant = new Effects::Constants::CShaderConstantVector3 (WallpaperEngine::Core::aToVector3 (*cur));
        }
        else
        {
            throw std::runtime_error ("unknown shader constant type");
        }

        pass->insertConstant (cur.key (), constant);
    }
}

void CEffect::fbosFromJSON (json::const_iterator fbos_it, CEffect* effect)
{
    auto cur = (*fbos_it).begin ();
    auto end = (*fbos_it).end ();

    for (; cur != end; cur ++)
    {
        effect->insertFBO (
            Effects::CFBO::fromJSON (*cur)
        );
    }
}

void CEffect::dependencyFromJSON (json::const_iterator dependencies_it, CEffect* effect)
{
    auto cur = (*dependencies_it).begin ();
    auto end = (*dependencies_it).end ();

    for (; cur != end; cur ++)
    {
        effect->insertDependency (*cur);
    }
}

void CEffect::materialsFromJSON (json::const_iterator passes_it, CEffect* effect, CContainer* container)
{
    auto cur = (*passes_it).begin ();
    auto end = (*passes_it).end ();

    for (; cur != end; cur ++)
    {
        auto materialfile = (*cur).find ("material");
        auto target = (*cur).find ("target");
        auto bind = (*cur).find ("bind");

        if (materialfile == (*cur).end ())
        {
            throw std::runtime_error ("Effect pass must have a material file");
        }

        Images::CMaterial* material = nullptr;

        if (target == (*cur).end ())
        {
            material = Images::CMaterial::fromFile ((*materialfile).get <std::string> (), container);
        }
        else
        {
            material = Images::CMaterial::fromFile ((*materialfile).get <std::string> (), *target, container);
        }

        if (bind != (*cur).end ())
        {
            auto bindCur = (*bind).begin ();
            auto bindEnd = (*bind).end ();

            for (; bindCur != bindEnd; bindCur ++)
            {
                material->insertTextureBind (
                    Effects::CBind::fromJSON (*bindCur)
                );
            }
        }

        effect->insertMaterial (material);
    }
}

const std::vector<std::string>& CEffect::getDependencies () const
{
    return this->m_dependencies;
}

const std::vector<Images::CMaterial*>& CEffect::getMaterials () const
{
    return this->m_materials;
}

const std::vector<Effects::CFBO*>& CEffect::getFbos () const
{
    return this->m_fbos;
}

Effects::CFBO* CEffect::findFBO (const std::string& name)
{
    auto cur = this->m_fbos.begin ();
    auto end = this->m_fbos.end ();

    for (; cur != end; cur ++)
    {
        if ((*cur)->getName () == name)
        {
            return (*cur);
        }
    }

    throw std::runtime_error ("cannot find fbo named " + name);
}

void CEffect::insertDependency (const std::string& dep)
{
    this->m_dependencies.push_back (dep);
}

void CEffect::insertMaterial (Images::CMaterial* material)
{
    this->m_materials.push_back (material);
}

void CEffect::insertFBO (Effects::CFBO* fbo)
{
    this->m_fbos.push_back (fbo);
}