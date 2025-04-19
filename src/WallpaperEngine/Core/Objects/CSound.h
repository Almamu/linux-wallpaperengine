#pragma once

#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"

namespace WallpaperEngine::Core::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Core::Objects {
using json = nlohmann::json;
using namespace WallpaperEngine::Core::UserSettings;

/**
 * Represents a sound played while the background is working
 */
class CSound : public CObject {
    friend class CObject;

  public:
    static const CObject* fromJSON (
        const Wallpapers::CScene* scene, const json& data, const CUserSettingBoolean* visible,
        int id, const std::string& name, const CUserSettingVector3* origin, const CUserSettingVector3* scale,
        const CUserSettingVector3* angles, std::vector<int> dependencies);

    /**
     * @return The list of sounds to play
     */
    [[nodiscard]] const std::vector<std::string>& getSounds () const;
    /**
     * @return If the sound should repeat or not
     */
    [[nodiscard]] bool isRepeat () const;

  protected:
    CSound (
        const Wallpapers::CScene* scene, const CUserSettingBoolean* visible, int id, std::string name,
        const CUserSettingVector3* origin, const CUserSettingVector3* scale, const CUserSettingVector3* angles,
        bool repeat, std::vector<std::string> sounds, std::vector<int> dependencies);

    /**
     * Type value used to differentiate the different types of objects in a background
     */
    static const std::string Type;

  private:
    /** If the sounds should repeat or not */
    bool m_repeat;
    /** The list of sounds to play */
    std::vector<std::string> m_sounds;
};
} // namespace WallpaperEngine::Core::Objects
