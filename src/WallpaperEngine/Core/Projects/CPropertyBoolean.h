#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;

    class CPropertyBoolean : public CProperty
    {
    public:
        static CPropertyBoolean* fromJSON (json data, const std::string& name);

        bool getValue ();

        static const std::string Type;

    private:
        CPropertyBoolean (bool value, const std::string& name, const std::string& text);

        bool m_value;
    };
};
