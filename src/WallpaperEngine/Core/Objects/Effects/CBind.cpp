#include "CBind.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects;

CBind::CBind (std::string name, uint32_t index) :
    m_name (std::move(name)),
    m_index (index) {}

const CBind* CBind::fromJSON (const json& data) {
    return new CBind (
        jsonFindRequired <std::string> (data, "name", "bind must have texture name"),
        jsonFindRequired <uint32_t> (data, "index", "bind must have index")
    );
}

const std::string& CBind::getName () const {
    return this->m_name;
}

const uint32_t& CBind::getIndex () const {
    return this->m_index;
}