#pragma once

#include "Core.h"

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Core/Objects/CEffect.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"

namespace WallpaperEngine::Core
{
    class CScene;
}

namespace WallpaperEngine::Core::Objects
{
    class CEffect;
}

namespace WallpaperEngine::Core::UserSettings
{
    class CUserSettingBoolean;
}

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;
    using namespace WallpaperEngine::Core::UserSettings;

    class CObject
    {
        friend class CScene;
    public:
        static CObject* fromJSON (json data, CScene* scene, const CContainer* container);

        template<class T> const T* as () const { assert (is <T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is <T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        const std::vector<Objects::CEffect*>& getEffects () const;
        const std::vector<uint32_t>& getDependencies () const;
        const int getId () const;

        glm::vec3 getOrigin () const;
        glm::vec3 getScale () const;
        const glm::vec3& getAngles () const;
        const std::string& getName () const;

        const bool isVisible () const;
        CScene* getScene () const;
    protected:
        CObject (
            CScene* scene,
            CUserSettingBoolean* visible,
            uint32_t id,
            std::string name,
            std::string type,
            CUserSettingVector3* origin,
            CUserSettingVector3* scale,
            const glm::vec3& angles
        );

        void insertEffect (Objects::CEffect* effect);
        void insertDependency (uint32_t dependency);
    private:
        std::string m_type;

        CUserSettingBoolean* m_visible;
        uint32_t m_id;
        std::string m_name;
        CUserSettingVector3* m_origin;
        CUserSettingVector3* m_scale;
        glm::vec3 m_angles;

        std::vector<Objects::CEffect*> m_effects;
        std::vector<uint32_t> m_dependencies;

        CScene* m_scene;
    };
};
