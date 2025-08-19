#include <sstream>
#include <utility>

#include "CPropertyBoolean.h"

using namespace WallpaperEngine::Core::Projects;

std::shared_ptr<CPropertyBoolean> CPropertyBoolean::fromJSON (const JSON& data, std::string name) {
    return std::make_shared <CPropertyBoolean> (
        data.require ("value", "Boolean property must have a value"),
        std::move(name),
        data.optional <std::string> ("text", "")
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

const char* CPropertyBoolean::getPropertyType () const {
    return "bool";
}

CPropertyBoolean::CPropertyBoolean (bool value, std::string name, std::string text) :
    CProperty (std::move(name), std::move(text)) {}
