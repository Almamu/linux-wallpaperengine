#pragma once

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/CObject.h"

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;

    class CSound : public CObject
    {
        friend class CObject;

    public:
        static CObject* fromJSON (
            json data,
            bool visible,
            uint32_t id,
            std::string name,
            const glm::vec3& origin,
            const glm::vec3& scale,
            const glm::vec3& angles
        );

        void insertSound (std::string filename);
        const std::vector<std::string>& getSounds () const;

    protected:
        CSound (
            bool visible,
            uint32_t id,
            std::string name,
            const glm::vec3& origin,
            const glm::vec3& scale,
            const glm::vec3& angles
        );

        static const std::string Type;
    private:
        std::vector<std::string> m_sounds;
    };
}
