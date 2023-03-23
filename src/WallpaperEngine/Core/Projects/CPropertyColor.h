#pragma once

#include "CProperty.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;

    /**
     * Represents a color property
     */
    class CPropertyColor : public CProperty
    {
    public:
        static CPropertyColor* fromJSON (json data, const std::string& name);

        /**
         * @return The RGB color value in the 0-1 range
         */
        [[nodiscard]] const glm::vec3& getValue () const;
        [[nodiscard]] std::string dump () const override;
        void update (const std::string& value) override;

        static const std::string Type;

    private:
        CPropertyColor (glm::vec3 color, const std::string& name, const std::string& text);

        glm::vec3 m_color;
    };
}
