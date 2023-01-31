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

        const FloatColor& getValue () const;
        std::string dump () const override;
        void update (const std::string& value) override;

        static const std::string Type;

    private:
        CPropertyColor (FloatColor color, const std::string& name, const std::string& text);

        FloatColor m_color;
    };
};
