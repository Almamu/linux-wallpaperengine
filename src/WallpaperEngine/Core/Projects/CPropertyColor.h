#pragma once

#include "CProperty.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Core::Types;

    class CPropertyColor : public CProperty
    {
    public:
        static CPropertyColor* fromJSON (json data, const std::string& name);

        const IntegerColor& getValue () const;

        static const std::string Type;

    private:
        CPropertyColor (IntegerColor color, const std::string& name, const std::string& text);

        IntegerColor m_color;
    };
};
