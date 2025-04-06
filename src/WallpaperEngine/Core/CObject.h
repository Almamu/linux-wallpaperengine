#pragma once

#include "Core.h"

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Core/Objects/CEffect.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"

namespace WallpaperEngine::Core::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Core::Objects {
class CEffect;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingBoolean;
}

namespace WallpaperEngine::Core {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Core::UserSettings;

class CObject {
    friend class Wallpapers::CScene;

  public:
    static CObject* fromJSON (json data, Wallpapers::CScene* scene, CContainer* container);

    template <class T> const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> bool is () {
        return this->m_type == T::Type;
    }

    [[nodiscard]] const std::vector<Objects::CEffect*>& getEffects () const;
    [[nodiscard]] const std::vector<int>& getDependencies () const;
    [[nodiscard]] int getId () const;

    [[nodiscard]] glm::vec3 getOrigin () const;
    [[nodiscard]] glm::vec3 getScale () const;
    [[nodiscard]] const glm::vec3& getAngles () const;
    [[nodiscard]] const std::string& getName () const;

    [[nodiscard]] bool isVisible () const;
    [[nodiscard]] Wallpapers::CScene* getScene () const;

  protected:
    CObject (Wallpapers::CScene* scene, CUserSettingBoolean* visible, int id, std::string name, std::string type,
             CUserSettingVector3* origin, CUserSettingVector3* scale, const glm::vec3& angles);

    void insertEffect (Objects::CEffect* effect);
    void insertDependency (int dependency);

  private:
    std::string m_type;

    CUserSettingBoolean* m_visible;
    int m_id;
    std::string m_name;
    CUserSettingVector3* m_origin;
    CUserSettingVector3* m_scale;
    glm::vec3 m_angles;

    std::vector<Objects::CEffect*> m_effects;
    std::vector<int> m_dependencies;

    Wallpapers::CScene* m_scene;
};
} // namespace WallpaperEngine::Core
