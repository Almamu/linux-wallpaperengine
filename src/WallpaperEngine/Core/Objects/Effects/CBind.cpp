#include "CBind.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects;

CBind::CBind (std::string name, irr::u32 index) :
    m_name (std::move(name)),
    m_index (index)
{
}

CBind* CBind::fromJSON (json data)
{
    auto name_it = jsonFindRequired (data, "name", "bind must have texture name");
    auto index_it = jsonFindRequired (data, "index", "bind must have index");

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