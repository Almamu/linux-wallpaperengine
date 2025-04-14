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
    static const CObject* fromJSON (const json& data, const Wallpapers::CScene* scene, const CContainer* container);

    template <class T> const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> bool is () const {
        return this->m_type == T::Type;
    }

    [[nodiscard]] const std::vector<int>& getDependencies () const;
    [[nodiscard]] int getId () const;

    [[nodiscard]] const glm::vec3& getOrigin () const;
    [[nodiscard]] const glm::vec3& getScale () const;
    [[nodiscard]] const glm::vec3& getAngles () const;
    [[nodiscard]] const std::string& getName () const;

    [[nodiscard]] bool isVisible () const;
    [[nodiscard]] const Wallpapers::CScene* getScene () const;

  protected:
    CObject (
        const Wallpapers::CScene* scene, const CUserSettingBoolean* visible, int id, std::string name, std::string type,
        const CUserSettingVector3* origin, const CUserSettingVector3* scale, glm::vec3 angles,
        std::vector<int> dependencies);

  private:
    const std::string m_type;

    const CUserSettingBoolean* m_visible;
    int m_id;
    const std::string m_name;
    const CUserSettingVector3* m_origin;
    const CUserSettingVector3* m_scale;
    const glm::vec3 m_angles;

    const std::vector<int> m_dependencies;

    const Wallpapers::CScene* m_scene;
};
} // namespace WallpaperEngine::Core
