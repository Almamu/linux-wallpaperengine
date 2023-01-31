#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;

    class CPropertyText : public CProperty
    {
    public:
        static CPropertyText* fromJSON (json data, const std::string& name);
        std::string dump () const override;
        void update (const std::string& value) override;

        static const std::string Type;

    private:
        CPropertyText (const std::string& name, const std::string& text);
    };
};
