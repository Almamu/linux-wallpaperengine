#pragma once

#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/CObject.h"

#include "WallpaperEngine/Assets/CContainer.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"

namespace WallpaperEngine::Core
{
    class CScene;
}

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;
    using namespace WallpaperEngine::Core::UserSettings;

    class CImage : public CObject
    {
        friend class CObject;
    public:
        static CObject* fromJSON (
            CScene* scene,
            json data,
            const CContainer* container,
            CUserSettingBoolean* visible,
            uint32_t id,
            std::string name,
            const glm::vec3& origin,
            const glm::vec3& scale,
            const glm::vec3& angles
        );

        const Images::CMaterial* getMaterial () const;
        const glm::vec2& getSize () const;
        const std::string& getAlignment () const;
        const float getAlpha () const;
        const glm::vec3& getColor () const;
        const float getBrightness () const;
        const uint32_t getColorBlendMode () const;
        const glm::vec2& getParallaxDepth () const;

    protected:
        CImage (
            CScene* scene,
            Images::CMaterial* material,
            CUserSettingBoolean* visible,
            uint32_t id,
            std::string name,
            const glm::vec3& origin,
            const glm::vec3& scale,
            const glm::vec3& angles,
            const glm::vec2& size,
            std::string alignment,
            const glm::vec3& color,
            float alpha,
            float brightness,
            uint32_t colorBlendMode,
            const glm::vec2& parallaxDepth
        );

        static const std::string Type;

    private:
        glm::vec2 m_size;
        const glm::vec2 m_parallaxDepth;
        Images::CMaterial* m_material;
        std::string m_alignment;
        float m_alpha;
        float m_brightness;
        glm::vec3 m_color;
        uint32_t m_colorBlendMode;
    };
};
