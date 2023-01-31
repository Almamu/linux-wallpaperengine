#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;

    class CPropertyComboValue
    {
    public:
        std::string label;
        std::string value;
    };

    class CPropertyCombo : public CProperty
    {
    public:
        static CPropertyCombo* fromJSON (json data, const std::string& name);

        const std::string& getValue () const;
        std::string dump () const override;
        void update (const std::string& value) override;

        static const std::string Type;

    private:
        CPropertyCombo (const std::string& name, const std::string& text, std::string  defaultValue);

        void addValue (std::string label, std::string value);

        std::vector <CPropertyComboValue*> m_values;
        std::string m_defaultValue;
    };
};
