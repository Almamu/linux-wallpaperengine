#include "CPropertyText.h"
#include "WallpaperEngine/Core/Core.h"
#include <sstream>

using namespace WallpaperEngine::Core::Projects;

const CPropertyText* CPropertyText::fromJSON (const json& data, std::string name) {
    const auto text = data.find ("type");

    return new CPropertyText (name, *text);
}

std::string CPropertyText::dump () const {
    std::stringstream ss;

    ss << this->m_name << " - text" << std::endl
       << "\t"
       << "Value: " << this->m_text;

    return ss.str ();
}

void CPropertyText::update (const std::string& value) const {
    this->m_text = value;
}

CPropertyText::CPropertyText (std::string name, std::string text) : CProperty (name, Type, text) {}

const std::string CPropertyText::Type = "text";