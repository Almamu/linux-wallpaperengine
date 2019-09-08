#include "CEffect.h"

#include <utility>

#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/Effects/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/CShaderConstantString.h"
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
    json::const_iterator file_it = data.find ("file");
    json::const_iterator effectpasses_it = data.find ("passes");

    if (file_it == data.end ())
    {
        throw std::runtime_error ("Object effect must have a file");
    }

    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile ((*file_it).get <std::string> ().c_str ()));

    json::const_iterator name_it = content.find ("name");
    json::const_iterator description_it = content.find ("description");
    json::const_iterator group_it = content.find ("group");
    json::const_iterator preview_it = content.find ("preview");
    json::const_iterator passes_it = content.find ("passes");
    json::const_iterator dependencies_it = content.find ("dependencies");

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

    json::const_iterator cur = (*passes_it).begin ();
    json::const_iterator end = (*passes_it).end ();

    for (; cur != end; cur ++)
    {
        json::const_iterator materialfile = (*cur).find ("material");

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
            json::const_iterator constants_it = (*cur).find ("constantshadervalues");

            if (constants_it == (*cur).end ())
                continue;

            json::const_iterator constantCur = (*constants_it).begin ();
            json::const_iterator constantEnd = (*constants_it).end ();

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
                    constant = new Effects::CShaderConstantString ((*constantCur).get <std::string> ());
                }
                else
                {
                    throw std::runtime_error ("unknown shader constant type");
                }

                effect->insertConstant (constantCur.key (), constant);
            }

            json::const_iterator textures_it = (*cur).find ("textures");

            if (textures_it == (*cur).end ())
                continue;

            Images::CMaterial* material = effect->getMaterials ()->at (passNumber);
            std::vector<Images::Materials::CPassess*>::const_iterator materialCur = material->getPasses ()->begin ();
            std::vector<Images::Materials::CPassess*>::const_iterator materialEnd = material->getPasses ()->end ();

            for (; materialCur != materialEnd; materialCur ++)
            {
                json::const_iterator texturesCur = (*textures_it).begin ();
                json::const_iterator texturesEnd = (*textures_it).end ();

                for (int textureNumber = 0; texturesCur != texturesEnd; texturesCur ++)
                {
                    std::string texture;

                    if ((*texturesCur).is_null () == true)
                    {
                        if (object->Is <CImage> () == false)
                        {
                            throw std::runtime_error ("unexpected null texture for non-image object");
                        }

                        CImage* image = object->As <CImage> ();

                        texture = (*(*image->getMaterial ()->getPasses ()->begin ())->getTextures ()->begin ());
                    }
                    else
                    {
                        texture = *texturesCur;
                    }

                    std::vector<std::string>* passTextures = (*materialCur)->getTextures ();

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

std::vector<std::string>* CEffect::getDependencies ()
{
    return &this->m_dependencies;
}

std::vector<Images::CMaterial*>* CEffect::getMaterials ()
{
    return &this->m_materials;
}

std::map<std::string, Effects::CShaderConstant*>* CEffect::getConstants ()
{
    return &this->m_constants;
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