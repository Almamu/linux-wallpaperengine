#include <WallpaperEngine/FileSystem/FileSystem.h>

#include "CProject.h"

using namespace WallpaperEngine::Core;

CProject::CProject (std::string title, std::string type, CScene *scene) :
    m_title (std::move (title)),
    m_type (std::move (type)),
    m_scene (scene)
{
    this->m_scene->setProject (this);
}

CProject* CProject::fromFile (const irr::io::path& filename)
{
    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename));

    json::const_iterator title = content.find ("title");
    json::const_iterator type = content.find ("type");
    json::const_iterator file = content.find ("file");
    json::const_iterator general = content.find ("general");

    if (title == content.end ())
    {
        throw std::runtime_error ("Project title missing");
    }

    if (type == content.end ())
    {
        throw std::runtime_error ("Project type missing");
    }

    if (file == content.end ())
    {
        throw std::runtime_error ("Project's main file missing");
    }

    CProject* project = new CProject (
        *title,
        *type,
        CScene::fromFile ((*file).get <std::string> ().c_str ())
    );

    if (general != content.end ())
    {
        json::const_iterator properties = (*general).find ("properties");

        if (properties != (*general).end ())
        {
            json::const_iterator cur = (*properties).begin ();
            json::const_iterator end = (*properties).end ();

            for (; cur != end; cur ++)
            {
                project->insertProperty (
                        Projects::CProperty::fromJSON (*cur, cur.key ())
                );
            }
        }
    }

    return project;
}

CScene* CProject::getScene ()
{
    return this->m_scene;
}

std::string CProject::getTitle ()
{
    return this->m_title;
}

std::string CProject::getType ()
{
    return this->m_type;
}

std::vector<Projects::CProperty*>* CProject::getProperties ()
{
    return &this->m_properties;
}

void CProject::insertProperty (Projects::CProperty* property)
{
    this->m_properties.push_back (property);
}