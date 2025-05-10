#include "CPropertyText.h"
#include "WallpaperEngine/Core/Core.h"
#include <sstream>
#include <utility>

using namespace WallpaperEngine::Core::Projects;

std::shared_ptr<CPropertyText> CPropertyText::fromJSON (const json& data, std::string name) {
    //TODO: VALIDATE THIS IS RIGHT
    return std::make_shared <CPropertyText> (std::move(name), *data.find ("type"));
}

std::string CPropertyText::dump () const {
    std::stringstream ss;

    ss << this->m_name << " - text" << std::endl
       << "\t"
       << "Value: " << this->m_text;

    return ss.str ();
}

void CPropertyText::set (const std::string& value) {
    this->m_text = value;
}

const char* CPropertyText::getType () const {
    return "text";
}

CPropertyText::CPropertyText (std::string name, std::string text) :
    CProperty (std::move(name), std::move(text)) {}
