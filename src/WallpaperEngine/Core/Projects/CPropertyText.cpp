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

CPropertyText::CPropertyText (const std::string& name, const std::string& text) :
    CProperty (name, Type, text)
{
}

const std::string CPropertyText::Type = "text";