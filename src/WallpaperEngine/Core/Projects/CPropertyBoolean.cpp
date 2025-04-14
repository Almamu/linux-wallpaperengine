#include <sstream>

#include "CPropertyBoolean.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Projects;

const CPropertyBoolean* CPropertyBoolean::fromJSON (const json& data, std::string name) {
    return new CPropertyBoolean (
        jsonFindRequired <bool> (data, "value", "Boolean property must have a value"),
        name,
        jsonFindDefault<std::string> (data, "text", "")
    );
}

bool CPropertyBoolean::getValue () const {
    return this->m_value;
}

void CPropertyBoolean::update (const std::string& value) const {
    this->m_value = value == "1" || value == "true";
}

std::string CPropertyBoolean::dump () const {
    std::stringstream ss;

    ss << this->m_name << " - boolean" << std::endl
       << "\t"
       << "Description: " << this->m_text << std::endl
       << "\t"
       << "Value: " << this->m_value;

    return ss.str ();
}

CPropertyBoolean::CPropertyBoolean (bool value, std::string name, std::string text) :
    CProperty (name, Type, text),
    m_value (value) {}

const std::string CPropertyBoolean::Type = "bool";