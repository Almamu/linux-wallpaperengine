#pragma once

#include "Core.h"

#include "WallpaperEngine/Core/Objects/CEffect.h"
#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Core::Objects
{
    class CEffect;
}

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;

    class CObject
    {
    public:
        static CObject* fromJSON (json data, CContainer* container);

        template<class T> const T* as () const { assert (is <T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is <T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        const std::vector<Objects::CEffect*>& getEffects () const;
        const std::vector<uint32_t>& getDependencies () const;
        const int getId () const;

        const glm::vec3& getOrigin () const;
        const glm::vec3& getScale () const;
        const glm::vec3& getAngles () const;
        const std::string& getName () const;

        const bool isVisible () const;
    protected:
        CObject (
            bool visible,
            uint32_t id,
            std::string name,
            std::string type,
            const glm::vec3& origin,
            const glm::vec3& scale,
            const glm::vec3& angles
        );

        void insertEffect (Objects::CEffect* effect);
        void insertDependency (uint32_t dependency);
    private:
        std::string m_type;

        bool m_visible;
        uint32_t m_id;
        std::string m_name;
        glm::vec3 m_origin;
        glm::vec3 m_scale;
        glm::vec3 m_angles;

        std::vector<Objects::CEffect*> m_effects;
        std::vector<uint32_t> m_dependencies;
    };
};
