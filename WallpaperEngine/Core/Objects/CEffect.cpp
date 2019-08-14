#include "CEffect.h"

#include <utility>

#include "../../fs/utils.h"

using namespace WallpaperEngine::Core::Objects;

CEffect::CEffect (
        std::string name,
        std::string description,
        std::string group,
        std::string preview):
    m_name (std::move(name)),
    m_description (std::move(description)),
    m_group (std::move(group)),
    m_preview (std::move(preview))
{
}

CEffect* CEffect::fromJSON (json data)
{
    json::const_iterator file_it = data.find ("file");

    if (file_it == data.end ())
    {
        throw std::runtime_error ("Object effect must have a file");
    }

    json content = json::parse (WallpaperEngine::fs::utils::loadFullFile ((*file_it).get <std::string> ().c_str ()));

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
        *preview_it
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

void CEffect::insertDependency (const std::string& dep)
{
    this->m_dependencies.push_back (dep);
}

void CEffect::insertMaterial (Images::CMaterial* material)
{
    this->m_materials.push_back (material);
}