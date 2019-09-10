#include "CEffect.h"

#include <utility>

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/Effects/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/CShaderConstantVector3.h"
#include "WallpaperEngine/Core/Objects/Effects/CShaderConstantInteger.h"

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

CEffect* CEffect::fromJSON (json data, Core::CObject* object)
{
    auto file_it = data.find ("file");
    auto effectpasses_it = data.find ("passes");

    if (file_it == data.end ())
    {
        throw std::runtime_error ("Object effect must have a file");
    }

    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile ((*file_it).get <std::string> ().c_str ()));

    auto name_it = content.find ("name");
    auto description_it = content.find ("description");
    auto group_it = content.find ("group");
    auto preview_it = content.find ("preview");
    auto passes_it = content.find ("passes");
    auto dependencies_it = content.find ("dependencies");

    if (name_it == content.end ())
    {
        throw std::runtime_error ("Effect must have a name");
    }

    if (description_it == content.end ())
    {
        throw std::runtime_error ("Effect must have a description");
    }

    if (group_it == content.end ())
    {
        throw std::runtime_error ("Effect must have a group");
    }

    if (preview_it == content.end ())
    {
        throw std::runtime_error ("Effect must have a preview");
    }

    if (passes_it == content.end ())
    {
        throw std::runtime_error ("Effect must have a pass list");
    }

    if (dependencies_it == content.end ())
    {
        throw std::runtime_error ("Effect must have dependencies");
    }

    CEffect* effect = new CEffect (
        *name_it,
        *description_it,
        *group_it,
        *preview_it,
        object
    );

    auto cur = (*passes_it).begin ();
    auto end = (*passes_it).end ();

    for (; cur != end; cur ++)
    {
        auto materialfile = (*cur).find ("material");

        if (materialfile == (*cur).end ())
        {
            throw std::runtime_error ("Effect pass must have a material file");
        }

        effect->insertMaterial (
                Images::CMaterial::fromFile ((*materialfile).get <std::string> ().c_str ())
        );
    }

    cur = (*dependencies_it).begin ();
    end = (*dependencies_it).end ();

    for (; cur != end; cur ++)
    {
        effect->insertDependency (*cur);
    }

    if (effectpasses_it != data.end ())
    {
        cur = (*effectpasses_it).begin ();
        end = (*effectpasses_it).end ();

        for (int passNumber = 0; cur != end; cur ++, passNumber ++)
        {
            auto constants_it = (*cur).find ("constantshadervalues");

            if (constants_it == (*cur).end ())
                continue;

            auto constantCur = (*constants_it).begin ();
            auto constantEnd = (*constants_it).end ();

            for (; constantCur != constantEnd; constantCur ++)
            {
                Effects::CShaderConstant* constant = nullptr;

                if ((*constantCur).is_number_float () == true)
                {
                    constant = new Effects::CShaderConstantFloat ((*constantCur).get <irr::f32> ());
                }
                else if ((*constantCur).is_number_integer () == true)
                {
                    constant = new Effects::CShaderConstantInteger ((*constantCur).get <irr::s32> ());
                }
                else if ((*constantCur).is_string () == true)
                {
                    constant = new Effects::CShaderConstantVector3 (WallpaperEngine::Core::ato3vf (*constantCur));
                }
                else
                {
                    throw std::runtime_error ("unknown shader constant type");
                }

                effect->insertConstant (constantCur.key (), constant);
            }

            auto textures_it = (*cur).find ("textures");

            if (textures_it == (*cur).end ())
                continue;

            Images::CMaterial* material = effect->getMaterials ().at (passNumber);
            auto passCur = material->getPasses ().begin ();
            auto passEnd = material->getPasses ().end ();

            for (; passCur != passEnd; passCur ++)
            {
                auto texturesCur = (*textures_it).begin ();
                auto texturesEnd = (*textures_it).end ();

                for (int textureNumber = 0; texturesCur != texturesEnd; texturesCur ++)
                {
                    std::string texture;

                    if ((*texturesCur).is_null () == true)
                    {
                        if (object->is<CImage>() == false)
                        {
                            throw std::runtime_error ("unexpected null texture for non-image object");
                        }

                        CImage* image = object->as<CImage>();

                        texture = (*(*image->getMaterial ()->getPasses ().begin ())->getTextures ()->begin ());
                    }
                    else
                    {
                        texture = *texturesCur;
                    }

                    std::vector<std::string>* passTextures = (*passCur)->getTextures ();

                    if (textureNumber < passTextures->size ())
                        passTextures->at (textureNumber) = texture;
                    else
                        passTextures->push_back (texture);

                    textureNumber ++;
                }
            }

        }
    }

    return effect;
}

const std::vector<std::string>& CEffect::getDependencies () const
{
    return this->m_dependencies;
}

const std::vector<Images::CMaterial*>& CEffect::getMaterials () const
{
    return this->m_materials;
}

const std::map<std::string, Effects::CShaderConstant*>& CEffect::getConstants () const
{
    return this->m_constants;
}

void CEffect::insertDependency (const std::string& dep)
{
    this->m_dependencies.push_back (dep);
}

void CEffect::insertMaterial (Images::CMaterial* material)
{
    this->m_materials.push_back (material);
}

void CEffect::insertConstant (const std::string& name, Effects::CShaderConstant* constant)
{
    this->m_constants.insert (std::pair <std::string, Effects::CShaderConstant*> (name, constant));
}