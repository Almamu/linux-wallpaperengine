#include "CBind.h"

#include "WallpaperEngine/Core/Core.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects;

CBind::CBind (std::string name, irr::u32 index) :
    m_name (std::move(name)),
    m_index (index)
{
}

CBind* CBind::fromJSON (json data)
{
    auto name_it = jsonFindValueRequired(&data, "name", "bind must have texture name");
    auto index_it = jsonFindValueRequired(&data, "index", "bind must have index");

    return new CBind (*name_it, *index_it);
}

const std::string& CBind::getName () const
{
    return this->m_name;
}

const irr::u32& CBind::getIndex () const
{
    return this->m_index;
}