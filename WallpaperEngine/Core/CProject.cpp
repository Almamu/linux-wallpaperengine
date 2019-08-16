#include <WallpaperEngine/FileSystem/utils.h>

#include "CProject.h"

#include "../FileSystem/utils.h"

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

    return new CProject (
            *title,
            *type,
            CScene::fromFile ((*file).get <std::string> ().c_str ())
    );
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