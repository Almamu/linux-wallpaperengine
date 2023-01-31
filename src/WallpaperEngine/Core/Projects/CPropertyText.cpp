#include <sstream>
#include "CPropertyText.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Projects;

CPropertyText* CPropertyText::fromJSON (json data, const std::string& name)
{
    json::const_iterator text = data.find ("type");

    return new CPropertyText (
        name,
        *text
    );
}

std::string CPropertyText::dump () const
{
    std::stringstream ss;

    ss
        << this->m_name << " - text" << std::endl
        << "\t" << "Value: " << this->m_text;

    return ss.str();
}

void CPropertyText::update (const std::string& value)
{
    this->m_text = value;
}

CPropertyText::CPropertyText (const std::string& name, const std::string& text) :
    CProperty (name, Type, text)
{
}

const std::string CPropertyText::Type = "text";