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

    auto title = jsonFindRequired (content, "title", "Project title missing");
    auto type = jsonFindRequired (content, "type", "Project type missing");
    auto file = jsonFindRequired (content, "file", "Project's main file missing");
    auto general = content.find ("general");

    CProject* project = new CProject (
        *title,
        *type,
        CScene::fromFile ((*file).get <std::string> ().c_str (), (*type).get <std::string> ().c_str())
    );

    if (general != content.end ())
    {
        auto properties = (*general).find ("properties");

        if (properties != (*general).end ())
        {
            auto cur = (*properties).begin ();
            auto end = (*properties).end ();

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

const CScene* CProject::getScene () const
{
    return this->m_scene;
}

const std::string& CProject::getTitle () const
{
    return this->m_title;
}

const std::string& CProject::getType () const
{
    return this->m_type;
}

const std::vector<Projects::CProperty*>& CProject::getProperties () const
{
    return this->m_properties;
}

void CProject::insertProperty (Projects::CProperty* property)
{
    this->m_properties.push_back (property);
}