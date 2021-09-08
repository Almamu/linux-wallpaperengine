#include "CBind.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects;

CBind::CBind (std::string name, uint32_t index) :
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

const uint32_t& CBind::getIndex () const
{
    return this->m_index;
}