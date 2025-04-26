#include <sstream>
#include <utility>

#include "CPropertyBoolean.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Projects;

CPropertyBoolean* CPropertyBoolean::fromJSON (const json& data, std::string name) {
    return new CPropertyBoolean (
        jsonFindRequired <bool> (data, "value", "Boolean property must have a value"),
        std::move(name),
        jsonFindDefault<std::string> (data, "text", "")
    );
}


void CPropertyBoolean::set (const std::string& value) {
    this->update (value == "1" || value == "true" || value == "on");
}

std::string CPropertyBoolean::dump () const {
    std::stringstream ss;

    ss << this->m_name << " - boolean" << std::endl
       << "\t"
       << "Description: " << this->m_text << std::endl
       << "\t"
       << "Value: " << &this->getBool ();

    return ss.str ();
}

const char* CPropertyBoolean::getType () const {
    return "bool";
}

CPropertyBoolean::CPropertyBoolean (bool value, std::string name, std::string text) :
    CProperty (std::move(name), std::move(text)) {}
